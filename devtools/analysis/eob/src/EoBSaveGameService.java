import java.io.*;
import java.nio.*;
import java.nio.charset.StandardCharsets;
import java.util.*;

/**
 * EoBSaveGameService: Service layer for loading and saving Eye of the Beholder 1 savegames.
 * This implementation targets the original DOS EOBDATA.SAV format.
 */
public class EoBSaveGameService {

    public static class EoBCharacter {
        public int id;
        public int flags;
        public String name;
        public int str, strMax, strExt, strExtMax;
        public int intel, intelMax;
        public int wis, wisMax;
        public int dex, dexMax;
        public int con, conMax;
        public int cha, chaMax;
        public int hp, hpMax;
        public int ac;
        public int disabledSlots;
        public int raceSex;
        public int cClass;
        public int alignment;
        public int portrait;
        public int food;
        public int[] level = new int[3];
        public long[] exp = new long[3];
        
        public byte[] mageSpells = new byte[80];
        public byte[] clericSpells = new byte[80];
        public long mageSpellsAvailableFlags;
        
        public int[] inventory = new int[27]; // Indices into the global items list
        
        public long[] timers = new long[10];
        public byte[] events = new byte[10];
        public byte[] effectsRemainder = new byte[4];
        public long effectFlags;
        public int damageTaken;
        public byte[] slotStatus = new byte[5];
        
        // Internal padding/unused in DOS EOB1 SAV
        public byte[] padding = new byte[6];
    }

    public static class SaveGame {
        public List<EoBCharacter> characters = new ArrayList<>();
        public int currentLevel;
        public int currentBlock;
        public int currentDirection;
        public int itemInHand;
        public int hasTempDataFlags;
        public int partyEffectFlags;
        
        public List<EoBItemLibrary.EoBItem> globalItems = new ArrayList<>();
        
        // Level temp data chunks (Level 1-12)
        public byte[][] levelTempData = new byte[13][2040];
    }

    private EoBItemLibrary itemLibrary;

    public EoBSaveGameService(EoBItemLibrary itemLibrary) {
        this.itemLibrary = itemLibrary;
    }

    public SaveGame loadSaveGame(File file) throws IOException {
        byte[] data = new byte[(int) file.length()];
        try (FileInputStream fis = new FileInputStream(file)) {
            fis.read(data);
        }

        ByteBuffer bb = ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN);
        SaveGame save = new SaveGame();

        // 1. Load 6 characters (243 bytes each in EOB1 DOS)
        // Byte breakdown: 1(id)+1(flags)+11(name)+14(stats)+1(hp)+1(hpMax)+7(bio)+
        //                 3(level)+12(exp)+4(skip)+30(mageSpells)+30(clericSpells)+
        //                 4(spellFlags)+54(inventory)+40(timers)+10(events)+
        //                 4(effectsRemainder)+4(effectFlags)+1(damageTaken)+5(slotStatus)+6(skip) = 243
        final int CHAR_RECORD_SIZE = 243;
        for (int i = 0; i < 6; i++) {
            int charStart = i * CHAR_RECORD_SIZE;
            bb.position(charStart);
            
            EoBCharacter c = new EoBCharacter();
            c.id = bb.get() & 0xFF;
            c.flags = bb.get() & 0xFF;
            byte[] nameBytes = new byte[11];
            bb.get(nameBytes);
            c.name = new String(nameBytes, StandardCharsets.US_ASCII).trim();
            
            // Stats (14 bytes)
            c.str = bb.get(); c.strMax = bb.get();
            c.strExt = bb.get(); c.strExtMax = bb.get();
            c.intel = bb.get(); c.intelMax = bb.get();
            c.wis = bb.get(); c.wisMax = bb.get();
            c.dex = bb.get(); c.dexMax = bb.get();
            c.con = bb.get(); c.conMax = bb.get();
            c.cha = bb.get(); c.chaMax = bb.get();
            
            // HP (2 bytes for EOB1)
            c.hp = bb.get() & 0xFF;
            c.hpMax = bb.get() & 0xFF;
            
            // Bio & Combat (7 bytes)
            c.ac = bb.get();
            c.disabledSlots = bb.get() & 0xFF;
            c.raceSex = bb.get() & 0xFF;
            c.cClass = bb.get() & 0xFF;
            c.alignment = bb.get() & 0xFF;
            c.portrait = bb.get();
            c.food = bb.get() & 0xFF;
            
            // Levels & Exp (15 bytes)
            for (int j = 0; j < 3; j++) c.level[j] = bb.get() & 0xFF;
            for (int j = 0; j < 3; j++) c.exp[j] = bb.getInt() & 0xFFFFFFFFL;
            
            bb.getInt(); // Skip 4 bytes (offset 51 to 55)
            
            // Spells: EoB1 stores 5 groups of 6 spells per level (30 bytes each).
            // The in-memory array has slots for 10 spells per level; only the first 6
            // of each group are stored in the file (matching C++ readOriginalSaveFile).
            for (int ii = 0; ii < 5; ii++) bb.get(c.mageSpells, ii * 10, 6);
            for (int ii = 0; ii < 5; ii++) bb.get(c.clericSpells, ii * 10, 6);
            
            c.mageSpellsAvailableFlags = bb.getInt() & 0xFFFFFFFFL; // 4 bytes (total 119 bytes so far)
            
            // Inventory: 27 x sint16LE (DOS little-endian). ByteBuffer is already in
            // LITTLE_ENDIAN mode, so getShort() reads correctly.
            bb.position(charStart + 119);
            for (int j = 0; j < 27; j++) {
                c.inventory[j] = bb.getShort() & 0xFFFF;
            }
            
            // Remaining (72 bytes)
            for (int j = 0; j < 10; j++) c.timers[j] = bb.getInt() & 0xFFFFFFFFL;
            bb.get(c.events);
            bb.get(c.effectsRemainder);
            c.effectFlags = bb.getInt() & 0xFFFFFFFFL;
            c.damageTaken = bb.get() & 0xFF;
            bb.get(c.slotStatus);
            bb.get(c.padding);
            
            save.characters.add(c);
        }

        // 2. Party Status (at offset 1458 = 6 * 243)
        bb.position(6 * CHAR_RECORD_SIZE);
        save.currentLevel = bb.getShort() & 0xFFFF;
        save.currentBlock = bb.getShort() & 0xFFFF;
        save.currentDirection = bb.getShort() & 0xFFFF;
        save.itemInHand = bb.getShort() & 0xFFFF;
        save.hasTempDataFlags = bb.getShort() & 0xFFFF;
        save.partyEffectFlags = bb.getShort() & 0xFFFF;

        // Skip _inf->loadState: EOB1 DOS writes 12 x uint32LE flags + 1 x uint32LE = 52 bytes
        bb.position(bb.position() + 52);

        // 3. Global Items (500 items)
        for (int i = 0; i < 500; i++) {
            EoBItemLibrary.EoBItem it = new EoBItemLibrary.EoBItem();
            it.id = i;
            it.nameUnid = bb.get() & 0xFF;
            it.nameId = bb.get() & 0xFF;
            it.flags = bb.get() & 0xFF;
            it.icon = bb.get();
            it.type = bb.get();
            it.pos = bb.get();
            it.block = bb.getShort();
            it.next = bb.getShort() & 0xFFFF;
            it.prev = bb.getShort() & 0xFFFF;
            it.level = bb.get() & 0xFF;
            it.value = bb.get();
            
            // nameUnid/nameId are indices into the item name string table, not into
            // the prototype item list — use getItemNameString, not getItemName.
            if (itemLibrary != null) {
                it.unidName = itemLibrary.getItemNameString(it.nameUnid);
                it.idName = itemLibrary.getItemNameString(it.nameId);
            }
            
            save.globalItems.add(it);
        }

        // 4. Level Temp Data (Map changes, monsters, etc.)
        // There are usually 12-13 chunks of 2040 bytes each.
        for (int i = 0; i < 13 && bb.remaining() >= 2040; i++) {
            bb.get(save.levelTempData[i]);
        }

        return save;
    }

    public void saveSaveGame(SaveGame save, File file) throws IOException {
        ByteBuffer bb = ByteBuffer.allocate(34000).order(ByteOrder.LITTLE_ENDIAN); // Standard EOB1 SAV size

        for (EoBCharacter c : save.characters) {
            bb.put((byte) c.id);
            bb.put((byte) c.flags);
            byte[] nameBytes = new byte[11];
            byte[] rawName = c.name.getBytes(StandardCharsets.US_ASCII);
            System.arraycopy(rawName, 0, nameBytes, 0, Math.min(rawName.length, 11));
            bb.put(nameBytes);
            bb.put((byte) c.str);
            bb.put((byte) c.strMax);
            bb.put((byte) c.strExt);
            bb.put((byte) c.strExtMax);
            bb.put((byte) c.intel);
            bb.put((byte) c.intelMax);
            bb.put((byte) c.wis);
            bb.put((byte) c.wisMax);
            bb.put((byte) c.dex);
            bb.put((byte) c.dexMax);
            bb.put((byte) c.con);
            bb.put((byte) c.conMax);
            bb.put((byte) c.cha);
            bb.put((byte) c.chaMax);
            bb.put((byte) c.hp);
            bb.put((byte) c.hpMax);
            bb.put((byte) c.ac);
            bb.put((byte) c.disabledSlots);
            bb.put((byte) c.raceSex);
            bb.put((byte) c.cClass);
            bb.put((byte) c.alignment);
            bb.put((byte) c.portrait);
            bb.put((byte) c.food);
            for (int v : c.level) bb.put((byte) v);
            for (long v : c.exp) bb.putInt((int) v);
            bb.putInt(0); // skip 4
            for (int j = 0; j < 5; j++) bb.put(c.mageSpells, j * 10, 6);
            for (int j = 0; j < 5; j++) bb.put(c.clericSpells, j * 10, 6);
            bb.putInt((int) c.mageSpellsAvailableFlags);
            for (int v : c.inventory) bb.putShort((short) v);
            for (long v : c.timers) bb.putInt((int) v);
            bb.put(c.events);
            bb.put(c.effectsRemainder);
            bb.putInt((int) c.effectFlags);
            bb.put((byte) c.damageTaken);
            bb.put(c.slotStatus);
            bb.put(c.padding);
        }

        bb.putShort((short) save.currentLevel);
        bb.putShort((short) save.currentBlock);
        bb.putShort((short) save.currentDirection);
        bb.putShort((short) save.itemInHand);
        bb.putShort((short) save.hasTempDataFlags);
        bb.putShort((short) save.partyEffectFlags);
        
        bb.position(bb.position() + 52); // skip inf state (12 x uint32 + 1 x uint32 = 52 bytes)

        for (EoBItemLibrary.EoBItem it : save.globalItems) {
            bb.put((byte) it.nameUnid);
            bb.put((byte) it.nameId);
            bb.put((byte) it.flags);
            bb.put((byte) it.icon);
            bb.put((byte) it.type);
            bb.put((byte) it.pos);
            bb.putShort((short) it.block);
            bb.putShort((short) it.next);
            bb.putShort((short) it.prev);
            bb.put((byte) it.level);
            bb.put((byte) it.value);
        }

        for (int i = 0; i < 13; i++) {
            bb.put(save.levelTempData[i]);
        }

        try (FileOutputStream fos = new FileOutputStream(file)) {
            fos.write(bb.array());
        }
    }

    /**
     * Debugging method to print all character-related information from the loaded savegame.
     */
    public void debugPrintCharacters(SaveGame save) {
        System.out.println("=== Eye of the Beholder Character Debug Dump ===");
        System.out.println("Party Location: Level " + save.currentLevel + ", Block " + save.currentBlock + ", Dir " + save.currentDirection);
        System.out.println("Item in Hand Index: " + save.itemInHand);
        
        for (int i = 0; i < save.characters.size(); i++) {
            EoBCharacter c = save.characters.get(i);
            if (c.id == 0xFF || c.name.isEmpty()) continue;

            System.out.println("\n--------------------------------------------------");
            System.out.println("Slot " + (i + 1) + ": " + c.name + " (ID: " + c.id + ", Flags: 0x" + Integer.toHexString(c.flags) + ")");
            System.out.println("Stats: STR: " + c.str + "/" + c.strMax + " (Ext: " + c.strExt + "/" + c.strExtMax + ")");
            System.out.println("       INT: " + c.intel + "/" + c.intelMax + "  WIS: " + c.wis + "/" + c.wisMax);
            System.out.println("       DEX: " + c.dex + "/" + c.dexMax + "  CON: " + c.con + "/" + c.conMax + "  CHA: " + c.cha + "/" + c.chaMax);
            System.out.println("Combat: HP: " + c.hp + "/" + c.hpMax + "  AC: " + c.ac + "  Food: " + c.food);
            System.out.println("Bio:    Race/Sex: " + c.raceSex + "  Class: " + c.cClass + "  Align: " + c.alignment + "  Portrait: " + c.portrait);
            System.out.println("Levels: " + c.level[0] + "/" + c.level[1] + "/" + c.level[2]);
            System.out.println("EXP:    " + c.exp[0] + " / " + c.exp[1] + " / " + c.exp[2]);
            
            System.out.println("Inventory:");
            for (int j = 0; j < 27; j++) {
                int itemIdx = c.inventory[j];
                if (itemIdx == 0xFFFF || itemIdx == 0) continue; // Skip empty slots

                // inventory[j] is an index into the savegame global items list.
                // Each global item has already had its name resolved from the name table.
                String itemName;
                if (itemIdx < save.globalItems.size()) {
                    EoBItemLibrary.EoBItem globalItem = save.globalItems.get(itemIdx);
                    itemName = (globalItem.unidName != null && !globalItem.unidName.isEmpty())
                            ? globalItem.unidName : "Item #" + itemIdx;
                } else {
                    itemName = "Out-of-range index " + itemIdx;
                }
                String slotName = getSlotName(j);
                System.out.printf("  [%-10s] (Idx: %3d) %s%n",
                    slotName, itemIdx, itemName);
            }
        }
        System.out.println("\n================================================");
    }

    private String getSlotName(int slot) {
        switch (slot) {
            case 0: return "Right Hand";
            case 1: return "Left Hand";
            case 16: return "Quiver";
            case 17: return "Body";
            case 18: return "Belt 1";
            case 19: return "Head";
            case 20: return "Neck";
            case 21: return "Boots";
            case 22: return "Belt 2";
            case 23: return "Belt 3";
            case 24: return "Bracers";
            case 25: return "Ring 1";
            case 26: return "Ring 2";
            default: return (slot >= 2 && slot <= 15) ? "Pack " + (slot - 1) : "Slot " + slot;
        }
    }

    public static void main(String[] args) {
        if (args.length < 2) {
            System.out.println("Usage: java EoBSaveGameService <game_path> <save_file>");
            return;
        }

        try {
            EoBItemLibrary lib = new EoBItemLibrary();
            lib.load(args[0]);
            
            EoBSaveGameService service = new EoBSaveGameService(lib);
            SaveGame save = service.loadSaveGame(new File(args[1]));
            
            service.debugPrintCharacters(save);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}

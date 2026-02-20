import java.io.*;
import java.nio.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.awt.image.*;
import javax.imageio.*;

public class EoBItemLibrary {

    public static class EoBItem {
        public int id;
        public int nameUnid;
        public int nameId;
        public int flags;
        public int icon;
        public int type;
        public int pos;
        public int block;
        public int next;
        public int prev;
        public int level;
        public int value;

        public String unidName;
        public String idName;

        @Override
        public String toString() {
            return String.format("ID: 0x%04X | Unid: %-20s | Id: %-20s | Type: %d | Icon: %d", id, unidName, idName, type, icon);
        }
    }

    private List<EoBItem> items = new ArrayList<>();
    private List<String> itemNames = new ArrayList<>();
    private int[] palette = new int[256];

    public void load(String gamePath) throws IOException {
        File dir = new File(gamePath);
        byte[] itemDat = findAndGetFile(dir, "ITEM.DAT");
        if (itemDat == null) {
            throw new IOException("ITEM.DAT not found");
        }
        parseItemDat(itemDat);

        byte[] palData = findAndGetFile(dir, "EOBPAL.COL");
        if (palData != null) {
            loadPalette(palData);
        }
    }

    private byte[] findAndGetFile(File dir, String target) throws IOException {
        for (File f : dir.listFiles((d, name) -> name.toUpperCase().endsWith(".PAK"))) {
            byte[] data = getFile(f, target);
            if (data != null) return data;
        }
        return null;
    }

    private void loadPalette(byte[] data) {
        for (int i = 0; i < 256; i++) {
            int r = (data[i * 3] & 0xFF) << 2;
            int g = (data[i * 3 + 1] & 0xFF) << 2;
            int b = (data[i * 3 + 2] & 0xFF) << 2;
            palette[i] = (0xFF << 24) | (r << 16) | (g << 8) | b;
        }
    }

    private void parseItemDat(byte[] data) {
        ByteBuffer bb = ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN);
        int numItems = bb.getShort() & 0xFFFF;

        for (int i = 0; i < numItems; i++) {
            EoBItem it = new EoBItem();
            it.id = i;
            it.nameUnid = bb.get() & 0xFF;
            it.nameId = bb.get() & 0xFF;
            it.flags = bb.get() & 0xFF;
            it.icon = bb.get();
            it.type = bb.get();
            it.pos = bb.get();
            it.block = bb.getShort();
            it.next = bb.getShort();
            it.prev = bb.getShort();
            it.level = bb.get() & 0xFF;
            it.value = bb.get();
            items.add(it);
        }

        int numNames = bb.getShort() & 0xFFFF;
        for (int i = 0; i < numNames; i++) {
            byte[] nameBytes = new byte[35];
            bb.get(nameBytes);
            String name = new String(nameBytes, StandardCharsets.US_ASCII).trim();
            itemNames.add(name);
        }

        for (EoBItem it : items) {
            it.unidName = (it.nameUnid < itemNames.size()) ? itemNames.get(it.nameUnid) : "Unknown";
            it.idName = (it.nameId < itemNames.size()) ? itemNames.get(it.nameId) : "Unknown";
        }
    }

    public void extractIcons(String gamePath, String outputDir) throws IOException {
        File dir = new File(gamePath);
        byte[] cpsData = findAndGetFile(dir, "ITEMICN.CPS");
        if (cpsData == null) throw new IOException("ITEMICN.CPS not found");

        byte[] decoded = decodeCPS(cpsData);
        
        File outDir = new File(outputDir);
        if (!outDir.exists()) outDir.mkdirs();

        int iconsPerRow = 20;
        int iconSize = 16;
        int scale = 5;
        
        Set<Integer> extractedIcons = new HashSet<>();
        for (EoBItem it : items) {
            if (it.icon < 0 || extractedIcons.contains(it.icon)) continue;
            
            int iconIndex = it.icon;
            int x = (iconIndex % iconsPerRow) * iconSize;
            int y = (iconIndex / iconsPerRow) * iconSize;
            
            if (x + iconSize > 320 || y + iconSize > 200) continue;

            BufferedImage img = new BufferedImage(iconSize * scale, iconSize * scale, BufferedImage.TYPE_INT_ARGB);
            for (int iy = 0; iy < iconSize; iy++) {
                for (int ix = 0; ix < iconSize; ix++) {
                    int pixel = decoded[(y + iy) * 320 + (x + ix)] & 0xFF;
                    int color = (pixel == 0) ? 0x00000000 : palette[pixel];
                    for (int sy = 0; sy < scale; sy++) {
                        for (int sx = 0; sx < scale; sx++) {
                            img.setRGB(ix * scale + sx, iy * scale + sy, color);
                        }
                    }
                }
            }
            
            String name = it.unidName.replaceAll("[^a-zA-Z0-9]", "_");
            File outFile = new File(outDir, String.format("icon_%03d_%s.png", iconIndex, name));
            ImageIO.write(img, "png", outFile);
            extractedIcons.add(iconIndex);
        }
        System.out.println("Extracted " + extractedIcons.size() + " icons to " + outputDir);
    }

    private byte[] decodeCPS(byte[] src) {
        ByteBuffer bb = ByteBuffer.wrap(src).order(ByteOrder.LITTLE_ENDIAN);
        bb.getShort(); // skip type
        int compType = bb.get() & 0xFF;
        bb.get(); // skip
        int uncompressedSize = bb.getInt();
        int palSize = bb.getShort() & 0xFFFF;
        
        byte[] dst = new byte[uncompressedSize];
        int srcPos = 10 + palSize;
        
        if (compType == 4) {
            decodeFrame4(src, srcPos, dst);
        } else if (compType == 0) {
            System.arraycopy(src, srcPos, dst, 0, uncompressedSize);
        }
        return dst;
    }

    private void decodeFrame4(byte[] src, int srcPos, byte[] dst) {
        int dstPos = 0;
        int dstSize = dst.length;
        
        while (dstPos < dstSize && srcPos < src.length) {
            int code = src[srcPos++] & 0xFF;
            if ((code & 0x80) == 0) {
                int len = ((code >> 4) & 0x07) + 3;
                int offs = ((code & 0x0F) << 8) | (src[srcPos++] & 0xFF);
                for (int i = 0; i < len && dstPos < dstSize; i++) {
                    dst[dstPos] = dst[dstPos - offs];
                    dstPos++;
                }
            } else if ((code & 0x40) != 0) {
                if (code == 0xFE) {
                    int len = (src[srcPos++] & 0xFF) | ((src[srcPos++] & 0xFF) << 8);
                    byte val = src[srcPos++];
                    for (int i = 0; i < len && dstPos < dstSize; i++) dst[dstPos++] = val;
                } else {
                    int len = (code & 0x3F) + 3;
                    if (code == 0xFF) {
                        len = (src[srcPos++] & 0xFF) | ((src[srcPos++] & 0xFF) << 8);
                    }
                    int offs = (src[srcPos++] & 0xFF) | ((src[srcPos++] & 0xFF) << 8);
                    for (int i = 0; i < len && dstPos < dstSize; i++) {
                        dst[dstPos] = dst[offs + i];
                        dstPos++;
                    }
                }
            } else if (code != 0x80) {
                int len = code & 0x3F;
                for (int i = 0; i < len && dstPos < dstSize; i++) dst[dstPos++] = src[srcPos++];
            } else {
                break;
            }
        }
    }

    public byte[] getFile(File pakFile, String targetFile) throws IOException {
        try (RandomAccessFile raf = new RandomAccessFile(pakFile, "r")) {
            long filesize = raf.length();
            long startOffset = readIntLE(raf) & 0xFFFFFFFFL;
            while (raf.getFilePointer() < startOffset) {
                String fileName = readNullTerminatedString(raf);
                if (fileName.isEmpty()) break;
                long nextEntryOffset = readIntLE(raf) & 0xFFFFFFFFL;
                long endOffset = nextEntryOffset;
                if (endOffset == 0 || endOffset > filesize || (endOffset < startOffset && endOffset != 0)) endOffset = filesize;
                if (fileName.equalsIgnoreCase(targetFile)) {
                    byte[] data = new byte[(int) (endOffset - startOffset)];
                    raf.seek(startOffset);
                    raf.readFully(data);
                    return data;
                }
                if (endOffset == filesize || nextEntryOffset == 0) break;
                startOffset = endOffset;
            }
        }
        return null;
    }

    private int readIntLE(RandomAccessFile raf) throws IOException {
        int b1 = raf.read(); int b2 = raf.read(); int b3 = raf.read(); int b4 = raf.read();
        return (b4 << 24) | (b3 << 16) | (b2 << 8) | b1;
    }

    private String readNullTerminatedString(RandomAccessFile raf) throws IOException {
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        int b;
        while ((b = raf.read()) > 0) baos.write(b);
        return baos.toString(StandardCharsets.US_ASCII.name());
    }

    public void dumpItems(String outputFile) throws IOException {
        try (PrintWriter writer = new PrintWriter(new FileWriter(outputFile))) {
            writer.println("Eye of the Beholder Item Dump");
            writer.println("=============================");
            writer.printf("%-6s | %-25s | %-25s | %-4s | %-4s | %-4s | %-4s | %-6s | %-6s | %-6s | %-5s | %-5s%n",
                    "Hex ID", "Unidentified Name", "Identified Name", "Type", "Icon", "Slot", "Flg", "Block", "Next", "Prev", "Lvl", "Val");
            writer.println("-------|---------------------------|---------------------------|------|------|------|------|--------|--------|--------|-------|-------");
            for (EoBItem it : items) {
                writer.printf("0x%04X | %-25s | %-25s | %-4d | %-4d | %-4d | 0x%02X | %-6d | %-6d | %-6d | %-5d | %-5d%n",
                        it.id, it.unidName, it.idName, it.type, it.icon, it.pos, it.flags, it.block, it.next, it.prev, it.level, it.value);
            }
        }
    }

    public void generateHtmlSite(String gamePath, String outputDir) throws IOException {
        File baseDir = new File(outputDir);
        if (!baseDir.exists()) baseDir.mkdirs();
        
        File iconsDir = new File(baseDir, "icons");
        extractIcons(gamePath, iconsDir.getAbsolutePath());
        
        File htmlFile = new File(baseDir, "index.html");
        try (PrintWriter writer = new PrintWriter(new FileWriter(htmlFile))) {
            writer.println("<!DOCTYPE html>");
            writer.println("<html><head><title>Eye of the Beholder Item Database</title>");
            writer.println("<style>");
            writer.println("body { font-family: sans-serif; background: #222; color: #eee; }");
            writer.println("table { border-collapse: collapse; width: 100%; }");
            writer.println("th, td { border: 1px solid #444; padding: 8px; text-align: left; }");
            writer.println("th { background: #333; }");
            writer.println("tr:nth-child(even) { background: #2a2a2a; }");
            writer.println("tr:hover { background: #444; }");
            writer.println(".icon-img { image-rendering: pixelated; width: 80px; height: 80px; }");
            writer.println("</style></head><body>");
            writer.println("<h1>Eye of the Beholder Item Database</h1>");
            writer.println("<table><thead><tr>");
            writer.println("<th>ID</th><th>Icon</th><th>Unidentified Name</th><th>Identified Name</th><th>Type</th><th>Slot</th><th>Flags</th><th>Value</th>");
            writer.println("</tr></thead><tbody>");
            
            for (EoBItem it : items) {
                String iconPath = String.format("icons/icon_%03d_%s.png", it.icon, it.unidName.replaceAll("[^a-zA-Z0-9]", "_"));
                boolean iconExists = new File(iconsDir, iconPath.substring(6)).exists();
                
                writer.println("<tr>");
                writer.printf("<td>0x%04X</td>%n", it.id);
                writer.printf("<td>%s</td>%n", iconExists ? "<img class='icon-img' src='" + iconPath + "'>" : "N/A");
                writer.printf("<td>%s</td>%n", it.unidName);
                writer.printf("<td>%s</td>%n", it.idName);
                writer.printf("<td>%d</td>%n", it.type);
                writer.printf("<td>%d</td>%n", it.pos);
                writer.printf("<td>0x%02X</td>%n", it.flags);
                writer.printf("<td>%d</td>%n", it.value);
                writer.println("</tr>");
            }
            
            writer.println("</tbody></table></body></html>");
        }
        System.out.println("Generated HTML site in: " + outputDir);
    }

    public List<EoBItem> getItems() { return items; }

    public String getItemName(int itemIndex) {
        if (itemIndex >= 0 && itemIndex < items.size()) {
            EoBItem it = items.get(itemIndex);
            return it.unidName + (it.idName.isEmpty() ? "" : " / " + it.idName);
        }
        return "Unknown (" + itemIndex + ")";
    }

    /**
     * Look up a name string directly by its index in the item name table.
     * Use this when you have a nameUnid/nameId value from an EoBItem (from ITEM.DAT
     * or from a savegame global item), since those values are indices into the name
     * table, not into the prototype item list.
     */
    public String getItemNameString(int nameIndex) {
        if (nameIndex >= 0 && nameIndex < itemNames.size()) {
            return itemNames.get(nameIndex);
        }
        return "Unknown (" + nameIndex + ")";
    }

    public static void main(String[] args) {
        if (args.length < 1) {
            System.out.println("Usage: java EoBItemLibrary <path_to_game_data> [output_file|--extract-icons outdir|--html outdir]");
            return;
        }
        try {
            EoBItemLibrary lib = new EoBItemLibrary();
            if (args.length >= 3 && args[1].equals("--html")) {
                lib.load(args[0]);
                lib.generateHtmlSite(args[0], args[2]);
                return;
            }
            if (args.length >= 3 && args[1].equals("--extract-icons")) {
                lib.load(args[0]);
                lib.extractIcons(args[0], args[2]);
                return;
            }

            lib.load(args[0]);
            if (args.length >= 2) {
                lib.dumpItems(args[1]);
                System.out.println("Dumped items to: " + args[1]);
            } else {
                for (EoBItem it : lib.getItems()) System.out.println(it);
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}

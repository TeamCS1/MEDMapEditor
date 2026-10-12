import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.nio.file.*;

// Applies re/names.tsv ("hexaddr<TAB>name") to the program. Never overwrites a name that was
// already given by hand unless the new name comes from this file.
public class ApplyNames extends GhidraScript {
    @Override
    public void run() throws Exception {
        int ok = 0, created = 0, bad = 0;
        for (String line : Files.readAllLines(Paths.get(getScriptArgs()[0]))) {
            if (line.startsWith("#") || line.trim().isEmpty()) continue;
            String[] p = line.split("\t");
            Address a = toAddr(Long.parseLong(p[0], 16));
            Function f = getFunctionAt(a);
            if (f == null) { f = createFunction(a, null); if (f != null) created++; }
            if (f == null) { bad++; continue; }
            try { f.setName(p[1], SourceType.USER_DEFINED); ok++; } catch (Exception e) { bad++; }
        }
        println("renamed " + ok + ", created " + created + ", failed " + bad);
    }
}

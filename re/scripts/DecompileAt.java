import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

// usage: DecompileAt.java <outfile> <hexaddr> [<hexaddr> ...]
// Decompiles the function that starts at (or contains) each address. A line listing the functions it calls
// is written before each one, so the call tree can be followed by hand.
public class DecompileAt extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        PrintWriter w = new PrintWriter(new File(a[0]));
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        Set<Long> done = new HashSet<>();
        for (int i = 1; i < a.length; i++) {
            Address addr = toAddr(Long.parseLong(a[i], 16));
            Function f = getFunctionContaining(addr);
            if (f == null) f = createFunction(addr, null);
            if (f == null) { w.println("// no function at " + addr); continue; }
            if (!done.add(f.getEntryPoint().getOffset())) continue;
            Set<String> callees = new TreeSet<>();
            for (Function c : f.getCalledFunctions(monitor)) callees.add(c.getEntryPoint().toString());
            w.println("// ==== " + f.getEntryPoint() + " " + f.getName() + " size=" + f.getBody().getNumAddresses());
            w.println("// calls: " + callees);
            DecompileResults res = di.decompileFunction(f, 120, monitor);
            w.println(res.decompileCompleted() ? res.getDecompiledFunction().getC() : "// decompile failed");
        }
        w.close();
    }
}

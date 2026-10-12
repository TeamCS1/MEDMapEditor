import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

// usage: XrefDecompile.java <outfile> <hexaddr> [<hexaddr> ...]
// For every address (for example a string in .data) finds the code that references it and decompiles each
// containing function once. Also prints the references, so callers can be followed by hand.
public class XrefDecompile extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        PrintWriter w = new PrintWriter(new File(a[0]));
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        Set<Long> done = new HashSet<>();
        for (int i = 1; i < a.length; i++) {
            Address target = toAddr(Long.parseLong(a[i], 16));
            w.println("##### references to " + target);
            for (Reference r : getReferencesTo(target)) {
                Function f = getFunctionContaining(r.getFromAddress());
                w.println("  from " + r.getFromAddress() + " in " + (f == null ? "(no function)" : f.getName() + " @ " + f.getEntryPoint()));
                if (f == null || !done.add(f.getEntryPoint().getOffset())) continue;
                DecompileResults res = di.decompileFunction(f, 90, monitor);
                w.println("// ==== " + f.getEntryPoint() + " " + f.getName() + " size=" + f.getBody().getNumAddresses());
                w.println(res.decompileCompleted() ? res.getDecompiledFunction().getC() : "// decompile failed");
            }
        }
        w.close();
    }
}

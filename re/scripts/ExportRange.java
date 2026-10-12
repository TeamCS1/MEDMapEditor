import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class ExportRange extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();           // outfile lo hi vtables.tsv
        long lo = Long.parseLong(a[1],16), hi = Long.parseLong(a[2],16);
        // label virtual methods: tsv "addr<TAB>Class::vfn_N"
        Map<Long,String> names = new HashMap<>();
        for (String line : java.nio.file.Files.readAllLines(new File(a[3]).toPath())) {
            String[] p = line.split("\t"); if (p.length==2) names.putIfAbsent(Long.parseLong(p[0],16), p[1]);
        }
        for (Map.Entry<Long,String> e : names.entrySet()) {
            Address ad = toAddr(e.getKey());
            if (getFunctionAt(ad) == null) createFunction(ad, null);
            Function f = getFunctionAt(ad);
            if (f != null && f.getName().startsWith("FUN_")) {
                try { f.setName(e.getValue().replace("::","__"), SourceType.USER_DEFINED); } catch (Exception ex) {}
            }
        }
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        PrintWriter w = new PrintWriter(new File(a[0]));
        int n=0, fail=0;
        FunctionIterator it = currentProgram.getFunctionManager().getFunctions(toAddr(lo), true);
        while (it.hasNext() && !monitor.isCancelled()) {
            Function f = it.next();
            if (f.getEntryPoint().getOffset() >= hi) break;
            DecompileResults r = di.decompileFunction(f, 30, monitor);
            w.println("// ==== " + f.getEntryPoint() + " " + f.getName() + " size=" + f.getBody().getNumAddresses());
            if (r.decompileCompleted()) w.println(r.getDecompiledFunction().getC()); else { w.println("// FAILED"); fail++; }
            n++;
        }
        w.close();
        println("decompiled " + n + " functions, failed " + fail);
    }
}

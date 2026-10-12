import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class ExportClasses extends GhidraScript {
    static final Set<String> APP = new HashSet<>(Arrays.asList(
        "CBookMarks","CColView","CDFFView","CFileTree","CGLView","CIDECTree","CIDEChooser","CIDEFinder","CIDEModel","CIDETree",
        "CIMG","CIMGFinder","CIMGTool","CIMGTree","CIMGTreeView","CINSTChooser","CIPLFinder","CIPLTree","CIPLViewer","CLODChooser",
        "CLODItem","CLODTree","CMapList","CMapView","CStats","CTexView","CTree1","CTree2","CTree3","CTree4","CTreeItem",
        "Emitter","IDEView","Intro","MainWindow","ProgressDialog","TextureView"));

    @Override
    public void run() throws Exception {
        String outDir = getScriptArgs().length > 0 ? getScriptArgs()[0] : "out";
        new File(outDir).mkdirs();
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        Map<String,List<Function>> byClass = new TreeMap<>();
        FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
        while (it.hasNext()) {
            Function f = it.next();
            Namespace ns = f.getParentNamespace();
            while (ns != null && !ns.isGlobal() && !APP.contains(ns.getName())) ns = ns.getParentNamespace();
            if (ns != null && !ns.isGlobal() && APP.contains(ns.getName()))
                byClass.computeIfAbsent(ns.getName(), k -> new ArrayList<>()).add(f);
        }
        println("classes with named functions: " + byClass.size());
        PrintWriter idx = new PrintWriter(new File(outDir, "_index.txt"));
        for (Map.Entry<String,List<Function>> e : byClass.entrySet()) {
            idx.println(e.getKey() + "\t" + e.getValue().size());
            PrintWriter w = new PrintWriter(new File(outDir, e.getKey() + ".c"));
            for (Function f : e.getValue()) {
                DecompileResults r = di.decompileFunction(f, 60, monitor);
                w.println("// " + f.getEntryPoint() + "  " + f.getName(true));
                if (r.decompileCompleted()) w.println(r.getDecompiledFunction().getC());
                else w.println("// decompile failed: " + r.getErrorMessage());
            }
            w.close();
        }
        idx.close();
    }
}

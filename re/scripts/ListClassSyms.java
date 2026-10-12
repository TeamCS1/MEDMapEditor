import ghidra.app.script.GhidraScript;
import ghidra.program.model.symbol.*;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import java.io.*;
import java.util.*;

public class ListClassSyms extends GhidraScript {
    @Override
    public void run() throws Exception {
        PrintWriter w = new PrintWriter(new File(getScriptArgs()[0]));
        Set<String> app = new HashSet<>(Arrays.asList("CBookMarks","CColView","CDFFView","CFileTree","CGLView","CIDECTree","CIDEChooser","CIDEFinder","CIDEModel","CIDETree","CIMG","CIMGFinder","CIMGTool","CIMGTree","CIMGTreeView","CINSTChooser","CIPLFinder","CIPLTree","CIPLViewer","CLODChooser","CLODItem","CLODTree","CMapList","CMapView","CStats","CTexView","CTree1","CTree2","CTree3","CTree4","CTreeItem","Emitter","IDEView","Intro","MainWindow","ProgressDialog","TextureView"));
        SymbolIterator it = currentProgram.getSymbolTable().getAllSymbols(true);
        int n=0;
        while (it.hasNext()) {
            Symbol s = it.next();
            Namespace ns = s.getParentNamespace();
            if (ns != null && app.contains(ns.getName())) {
                w.println(ns.getName()+"\t"+s.getName()+"\t"+s.getAddress()+"\t"+s.getSymbolType());
                n++;
            }
        }
        w.close();
        println("app-class symbols: "+n+"  functions total: "+currentProgram.getFunctionManager().getFunctionCount());
    }
}

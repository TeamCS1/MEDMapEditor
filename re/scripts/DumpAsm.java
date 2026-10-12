import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import java.io.*;

// usage: DumpAsm.java <outfile> <hexstart> <hexend>   prints instructions with resolved call targets
public class DumpAsm extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        PrintWriter w = new PrintWriter(new File(a[0]));
        Address s = toAddr(Long.parseLong(a[1], 16)), e = toAddr(Long.parseLong(a[2], 16));
        for (Instruction i = getInstructionAt(s); i != null && i.getAddress().compareTo(e) <= 0; i = i.getNext()) {
            String t = i.toString();
            if (i.getFlowType().isCall() && i.getReferencesFrom().length > 0) {
                ghidra.program.model.symbol.Symbol sym = getSymbolAt(i.getReferencesFrom()[0].getToAddress());
                if (sym != null) t += "   ; " + sym.getName();
            }
            w.println(i.getAddress() + "  " + t);
        }
        w.close();
    }
}

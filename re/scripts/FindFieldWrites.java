import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.lang.Register;
import java.io.*;
import java.util.*;

// usage: FindFieldWrites.java <outfile> <hex displacement>
// Lists every instruction that writes to [register + displacement] with a 16-bit move, with the function it is in.
// Used to find where a field of a structure (here the model info) gets its value.
public class FindFieldWrites extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        long disp = Long.parseLong(a[1], 16);
        PrintWriter w = new PrintWriter(new File(a[0]));
        Map<String, List<String>> byFunction = new TreeMap<>();
        InstructionIterator it = currentProgram.getListing().getInstructions(true);
        while (it.hasNext()) {
            Instruction i = it.next();
            if (!i.getMnemonicString().equals("MOV") || i.getNumOperands() != 2) continue;
            String op0 = i.getDefaultOperandRepresentation(0);
            // a memory destination with the wanted displacement, e.g. "word ptr [ECX + 0x24]"
            if (!op0.startsWith("word ptr") && !op0.startsWith("dword ptr")) continue;
            String hex = String.format("0x%x", disp);
            if (!(op0.contains("+ " + hex + "]") || op0.contains("+ " + hex + ","))) continue;
            Function f = getFunctionContaining(i.getAddress());
            String key = f == null ? "(none)" : f.getEntryPoint() + " " + f.getName();
            byFunction.computeIfAbsent(key, k -> new ArrayList<>()).add(i.getAddress() + "  " + i.toString());
        }
        for (Map.Entry<String, List<String>> e : byFunction.entrySet()) {
            w.println("== " + e.getKey() + "  (" + e.getValue().size() + ")");
            for (String s : e.getValue()) w.println("   " + s);
        }
        w.close();
    }
}

using System;
using System.IO;
using PT3Play;

class Program
{
    static void Main(string[] args)
    {
        if (args.Length < 3)
        {
            Console.Error.WriteLine("usage: pt3oracle <in.pt3> <frames> <out.csv>");
            Environment.Exit(2);
        }
        string path = args[0];
        int frames = int.Parse(args[1]);
        string outPath = args[2];

        byte[] data = File.ReadAllBytes(path);

        var info = new AYSongInfo();
        info.Song = new AYSong[2];
        info.Song[0] = new AYSong { Module = data };
        info.Song[1] = new AYSong { Module = new byte[data.Length] };
        info.Chip = new AYChip[2];
        info.ModuleLength = data.Length;
        info.Is2AY = false;

        global::PT3Play.PT3Play.PT3_Init(info);

        // PT3DBG=A|B|C : trace l'etat interne d'UNE voie, frame par frame,
        // sur stderr -- c'est ce qui a permis de localiser les trois bugs
        // de spec.md §5.7 (chercher une valeur qui "saute" ou stagne d'une
        // frame a l'autre, comparer au meme champ cote pt3.py). PT3DBGN
        // borne le nombre de frames tracees (defaut 30 ; une trace complete
        // de plusieurs milliers de lignes n'aide personne).
        string dbgChan = Environment.GetEnvironmentVariable("PT3DBG");
        int dbgN = int.TryParse(Environment.GetEnvironmentVariable("PT3DBGN"), out var n0) ? n0 : 30;

        using (var w = new StreamWriter(outPath))
        {
            w.WriteLine("frame,r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12,r13");
            for (int f = 0; f < frames; f++)
            {
                global::PT3Play.PT3Play.PT3_Play(info);
                var regs = info.Chip[0].Reg;
                w.Write(f);
                for (int r = 0; r < 14; r++) { w.Write(","); w.Write(regs[r]); }
                w.WriteLine();
                if (!string.IsNullOrEmpty(dbgChan) && f < dbgN)
                {
                    var d = info.Song[0].Data;
                    var b = dbgChan == "A" ? d.PT3_A : dbgChan == "B" ? d.PT3_B : d.PT3_C;
                    Console.Error.WriteLine(
                        "f=" + f + " note=" + b.Note + " samPos=" + b.Position_In_Sample +
                        " sampLen=" + b.Sample_Length + " loopSamp=" + b.Loop_Sample_Position +
                        " ornPos=" + b.Position_In_Ornament + " tonAcc=" + b.Ton_Accumulator +
                        " ton=" + b.Ton + " sampPtr=" + b.SamplePointer +
                        " addr=" + b.Address_In_Pattern + " nsc=" + b.Note_Skip_Counter +
                        " delay=" + d.PT3.Delay + " dc=" + d.PT3.DelayCounter);
                }
            }
        }

        Console.Error.WriteLine("Is2AY=" + info.Is2AY + "  wrote " + frames + " frames to " + outPath);
    }
}

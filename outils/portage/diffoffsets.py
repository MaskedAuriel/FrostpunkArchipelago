# diffoffsets.py <epic> <steam> ... : offsets [reg + 0x..] et immediats propres a chaque version.
import sys, re, os, pickle, collections
ICI = os.path.dirname(os.path.abspath(__file__))
E = pickle.load(open(os.path.join(ICI, 'epic.pkl'), 'rb'))['fonctions']
S = pickle.load(open(os.path.join(ICI, 'steam.pkl'), 'rb'))['fonctions']
num = re.compile(r'(?<![\w*])0x[0-9a-f]+')
args = sys.argv[1:]
for i in range(0, len(args), 2):
    e, s = int(args[i], 16), int(args[i + 1], 16)
    ce = collections.Counter(n for ins in E[e][1] if 'rsp' not in ins and 'rbp' not in ins for n in num.findall(ins))
    cs = collections.Counter(n for ins in S[s][1] if 'rsp' not in ins and 'rbp' not in ins for n in num.findall(ins))
    pe = sorted(set(ce) - set(cs), key=lambda x: int(x, 16)); ps = sorted(set(cs) - set(ce), key=lambda x: int(x, 16))
    print('%07X (%d ins) / %07X (%d ins)\n   Epic seul : %s\n   Steam seul: %s' % (e, len(E[e][1]), s, len(S[s][1]), ' '.join(pe[:40]), ' '.join(ps[:40])))

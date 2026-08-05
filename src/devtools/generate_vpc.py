#!/usr/bin/env python3
import argparse
from pathlib import Path
from collections import defaultdict

TYPE_INFO={
 "lib":("OUTLIBDIR","$LIBCOMMON","source_lib_base.vpc"),
 "dll":("OUTBINDIR","$BINDIR","source_dll_base.vpc"),
 "exe":("OUTBINDIR","$BINDIR","source_exe_base.vpc"),
}

def split(v): return [x.strip() for x in v.split(";") if x.strip()] if v else []

def tree():
    return defaultdict(tree)

def insert(t,parts,file):
    n=t
    for p in parts:
        n=n[p]
    n.setdefault("__files__",[]).append(file)

def emit(f,node,indent):
    files=node.get("__files__",[])
    for x in sorted(files):
        f.write("\t" * indent + f'$File "{x}"\n')
    for k in sorted([k for k in node if k!="__files__"]):
        f.write("\t"*indent+f'$Folder "{k}"\n')
        f.write("\t"*indent+"{\n")
        emit(f,node[k],indent+1)
        f.write("\t"*indent+"}\n")

ap=argparse.ArgumentParser()
ap.add_argument("project")
ap.add_argument("--type",choices=["lib","dll","exe"],default="dll")
ap.add_argument("--root",default=".")
ap.add_argument("--srcdir",default=".")
ap.add_argument("--output")
ap.add_argument("--include",default="")
ap.add_argument("--defines",default="")
ap.add_argument("--libs",default="")
ap.add_argument("--exclude",default="")
ap.add_argument("--verbose",action="store_true")
args=ap.parse_args()

root=Path(args.root).resolve()
excl=set(split(args.exclude))
src_tree=tree(); hdr_tree=tree()
exts={".c",".cpp",".h",".hpp"}
count=0
for p in root.rglob("*"):
    if not p.is_file() or p.suffix.lower() not in exts: continue
    rel=p.relative_to(root)
    if any(part in excl for part in rel.parts): continue
    pos=rel.as_posix()
    if args.verbose: print(pos)
    count+=1
    folder=list(rel.parts[:-1])
    if p.suffix.lower() in {".h",".hpp"}:
        insert(hdr_tree,folder,pos)
    else:
        insert(src_tree,folder,pos)

out=args.output or args.project+".vpc"
macro,val,inc=TYPE_INFO[args.type]
with open(out,"w",encoding="utf8",newline="\n") as f:
    f.write(f'//-----------------------------------------------------------------------------\n')
    f.write(f'//\t{args.project+".vpc"}\n')
    f.write(f'//\n')
    f.write(f'//\tProject Script\n')
    f.write(f'//-----------------------------------------------------------------------------\n\n')
    srcdir = args.srcdir.replace("/", "\\")
    f.write(f'$Macro SRCDIR "{srcdir}"\n')
    f.write(f'$Macro {macro}\t"{val}"\n')
    f.write(f'$Include "$SRCDIR\\vpc_scripts\\{inc}"\n\n')

    if args.include or args.defines or args.libs:
        f.write("$Configuration\n{\n")
        f.write("\t$Compiler\n\t{\n")
        incs = split(args.include)
        if incs:
            value = "$BASE;" + ";".join(incs)
            f.write(f'\t\t$AdditionalIncludeDirectories "{value}"\n')
        defs = split(args.defines)
        if defs:
            value = "$BASE;" + ";".join(defs)
            f.write(f'\t\t$PreprocessorDefinitions "{value}"\n')
        f.write("\t}\n")
        if (args.type == "exe" or args.type == "dll"):
            f.write("\t$Linker\n\t{\n")
        else:
            f.write("\t$Librarian\n\t{\n")
        
        libs = split(args.libs)
        if libs:
            value = "$BASE " + " ".join(libs)
            f.write(f'\t\t$AdditionalDependencies "{value}"\n')
        f.write("\t}\n")
        f.write("}\n\n")

    f.write(f'$Project "{args.project}"\n{{\n')
    f.write('\t$Folder "Header Files"\n\t{\n')
    emit(f,hdr_tree,2)
    f.write("\t}\n\n")
    f.write('\t$Folder "Source Files"\n\t{\n')
    emit(f,src_tree,2)
    f.write("\t}\n}\n")
print(f"Generated {out} ({count} files)")

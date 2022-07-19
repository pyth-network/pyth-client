from email import contentmanager
import sys
import os 
"""
This script adds the traits BorshSerialize and BorshDeserialize to the
structs in the file provided in the first argument, whose names are in the 
list of remaining arguments
"""

BORSCH_TRAITS = "BorshSerialize, BorshDeserialize, "
USE_BORSCH = "use borsh::{BorshDeserialize, BorshSerialize};"
def get_struct_line(struct_name):
    """
    given the tyope name, guesses the line it is defined in
    """
    return "pub struct " + struct_name + " {"

#Read the content into a list of lines
bindings_path = sys.argv[1]
f = open(bindings_path, "r")
content = f.read()
f.close()
content = content.split("\n")

#for each provided name, insert BORSCH_TRAITS as the first two traits
for struct_name in sys.argv[2:]:
    expected_line = get_struct_line(struct_name)
    line_num = content.index(expected_line)
    assert(line_num != 0)#bingen always adds repr(c)
    if "derive" not in content[line_num - 1]:
        print("warning: bingen did not derive any traits for this type" + struct_name)
        content.insert(line_num, "#[derive(BorshSerialize, BorshDeserialize, Debug, Copy, Clone)]")
    else:
        derive_line = content[line_num - 1]
        if BORSCH_TRAITS in derive_line:
            continue
        traits_start = content[line_num - 1].index("(") + 1
        new_derive_line = derive_line[:traits_start] + BORSCH_TRAITS + derive_line[traits_start:]
        content[line_num - 1] = new_derive_line

#check if we need to add use borsch
if content[0] != USE_BORSCH:
    content.insert(0, USE_BORSCH)

#write back to the file
f = open(bindings_path, "w")
f.write("\n".join(content))
f.close()
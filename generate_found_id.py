# This tool will generate a found id header file for how an elf object should be loaded by the elf loader

# Usage: generate_found_id.py in_file cap_size num_entries

# The layout expected matches that generated by elf_loader.c.
# It is what the elf specifies, plus space for an initial TLS segment

import sys
import elftools
import hashlib
from elftools.elf.elffile import ELFFile

def main():

    in_path = sys.argv[1]
    cap_size = int(sys.argv[2])
    found_entries = sys.argv[3]

    name =  in_path.split(".elf")[0].split('/')[-1] + "_elf"
    define_guard = name + "_H"

    sha = hashlib.sha256()

    tls_seg_size = 0
    virt_addr_ptr = 0

    with open(in_path, 'rb') as in_file:

        elffile = ELFFile(in_file)
        entry = elffile.header['e_entry']


        for segment in elffile.iter_segments():

            seg_type = segment.header['p_type']
            mem_size = segment.header['p_memsz']
            if(seg_type == 'PT_LOAD'):

                fil_size = segment.header['p_filesz']
                virt_addr = segment.header['p_vaddr']

                # Cant go backwards. Why arn't your program headers sorted?
                if(virt_addr < virt_addr_ptr):
                    raise()

                # Skip bytes
                if(virt_addr > virt_addr_ptr):
                    sha.update(bytearray(virt_addr - virt_addr_ptr))

                # Then file bytes
                sha.update(segment.data())

                # Then padding with zeros
                if(mem_size > fil_size):
                    sha.update(bytearray(mem_size - fil_size))

                virt_addr_ptr = virt_addr + mem_size

            elif(seg_type == 'PT_TLS'):
                tls_seg_size = mem_size

    #Program loader adds in a tls seg size + CAP_SIZE of extra zeros

    tls_seg_size += cap_size
    virt_addr_ptr += tls_seg_size

    sha.update(bytearray(tls_seg_size))

    digest = sha.digest()

    print ("// DO NOT EDIT. AUTO-GENERATED.")
    print ("#ifndef " + define_guard)
    print ("#define " + define_guard)

    print ("#include \"nano/nanotypes.h\"")
    print ("static const found_id_t " + name + "_id = (found_id_t){")
    print ("    .sha256 = {" + ",".join("0x{:02x}".format(c) for c in digest) + "},")
    print ("    .length = " + str(virt_addr_ptr) + ",")
    print ("    .e0 = " + str(entry) + ",")
    print ("    .nentries = " + found_entries)
    print ("};")
    print ("#endif // " + define_guard)


if __name__== "__main__":
    main()

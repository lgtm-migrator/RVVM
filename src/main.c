/*
main.c - RVVM Entry point, API example
Copyright (C) 2021  LekKit <github.com/LekKit>
                    cerg2010cerg2010 <github.com/cerg2010cerg2010>
                    Mr0maks <mr.maks0443@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "compiler.h"
#include "mem_ops.h"
#include "rvvm.h"

#include <stdio.h>

#include "devices/clint.h"
#include "devices/plic.h"
#include "devices/ns16550a.h"
#include "devices/ata.h"
#include "devices/fb_window.h"
#include "devices/ps2-altera.h"
#include "devices/ps2-keyboard.h"
#include "devices/ps2-mouse.h"
#include "devices/syscon.h"
#include "devices/rtc-goldfish.h"
#include "devices/pci-bus.h"

#ifdef _WIN32
// For unicode fix
#include <windows.h>
#endif

#ifdef USE_NET
#include "devices/eth-oc.h"
#endif

#ifndef VERSION
#define VERSION "v0.4"
#endif

typedef struct {
    const char* bootrom;
    const char* kernel;
    const char* dtb;
    const char* dumpdtb;
    const char* image;
    size_t mem;
    uint32_t smp;
    uint32_t fb_x;
    uint32_t fb_y;
    bool rv64;
    bool sbi_align_fix;
    bool nogui;
} vm_args_t;

static size_t get_arg(const char** argv, const char** arg_name, const char** arg_val)
{
    if (argv[0][0] == '-') {
        size_t offset = (argv[0][1] == '-') ? 2 : 1;
        *arg_name = &argv[0][offset];
        for (size_t i=0; argv[0][offset + i] != 0; ++i) {
            if (argv[0][offset + i] == '=') {
                // Argument format -arg=val
                *arg_val = &argv[0][offset + i + 1];
                return 1;
            }
        }

        if (argv[1] == NULL || argv[1][0] == '-') {
            // Argument format -arg
            *arg_val = "";
            return 1;
        } else {
            // Argument format -arg val
            *arg_val = argv[1];
            return 2;
        }
    } else {
        *arg_name = "bootrom";
        *arg_val = argv[0];
        return 1;
    }
}

static inline bool cmp_arg(const char* arg, const char* name)
{
    for (size_t i=0; arg[i] != 0 && arg[i] != '='; ++i) {
        if (arg[i] != name[i]) return false;
    }
    return true;
}

static void print_help()
{
#ifdef _WIN32
    const wchar_t* help = L"\n"
#else
    printf("\n"
#endif
           "  ██▀███   ██▒   █▓ ██▒   █▓ ███▄ ▄███▓\n"
           " ▓██ ▒ ██▒▓██░   █▒▓██░   █▒▓██▒▀█▀ ██▒\n"
           " ▓██ ░▄█ ▒ ▓██  █▒░ ▓██  █▒░▓██    ▓██░\n"
           " ▒██▀▀█▄    ▒██ █░░  ▒██ █░░▒██    ▒██ \n"
           " ░██▓ ▒██▒   ▒▀█░     ▒▀█░  ▒██▒   ░██▒\n"
           " ░ ▒▓ ░▒▓░   ░ ▐░     ░ ▐░  ░ ▒░   ░  ░\n"
           "   ░▒ ░ ▒░   ░ ░░     ░ ░░  ░  ░      ░\n"
           "   ░░   ░      ░░       ░░  ░      ░   \n"
           "    ░           ░        ░         ░   \n"
           "               ░        ░              \n"
           "\n"
           "https://github.com/LekKit/RVVM ("VERSION")\n"
           "\n"
           "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\n"
           "This is free software: you are free to change and redistribute it.\n"
           "There is NO WARRANTY, to the extent permitted by law.\n"
           "\n"
           "Usage: rvvm [-mem 256M] [-smp 1] [-kernel ...] ... [bootrom]\n"
           "\n"
           "    -mem <amount>    Memory amount, default: 256M\n"
           "    -smp <count>     Cores count, default: 1\n"
#ifdef USE_RV64
           "    -rv64            Enable 64-bit RISC-V, 32-bit by default\n"
#endif
           "    -kernel <file>   Load kernel Image as SBI payload\n"
           "    -image <file>    Attach hard drive with raw image\n"
#ifdef USE_FB
           "    -res 1280x720    Change framebuffer resoulution\n"
           "    -nogui           Disable framebuffer & mouse/keyboard\n"
#endif
           "    -dtb <file>      Pass custom DTB to the machine\n"
#ifdef USE_FDT
           "    -dumpdtb <file>  Dump autogenerated DTB to file\n"
#endif
#ifdef USE_JIT
           "    -nojit           Disable RVJIT\n"
           "    -jitcache 16M    Per-core JIT cache size\n"
#endif
           "    -verbose         Enable verbose logging\n"
           "    -help            Show this help message\n"
           "    [bootrom]        Machine bootrom (SBI, BBL, etc)\n"
#ifdef _WIN32
           "\n";
    WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), help, wcslen(help), NULL, NULL);
#else
           "\n");
#endif
}

static bool parse_args(int argc, const char** argv, vm_args_t* args)
{
    const char* arg_name = "";
    const char* arg_val = "";
    uint8_t argpair;

    // Default params: 1 core, 256M ram, 640x480 screen
    args->smp = 1;
    args->mem = 256 << 20;
    args->fb_x = 640;
    args->fb_y = 480;

    for (int i=1; i<argc;) {
        argpair = get_arg(argv + i, &arg_name, &arg_val);
        i += argpair;
        if (cmp_arg(arg_name, "dtb")) {
            args->dtb = arg_val;
        } else if (cmp_arg(arg_name, "image")) {
            args->image = arg_val;
        } else if (cmp_arg(arg_name, "bootrom")) {
            args->bootrom = arg_val;
        } else if (cmp_arg(arg_name, "kernel")) {
            args->kernel = arg_val;
        } else if (cmp_arg(arg_name, "mem")) {
            if (strlen(arg_val))
                args->mem = ((size_t)atoi(arg_val)) << mem_suffix_shift(arg_val[strlen(arg_val)-1]);
        } else if (cmp_arg(arg_name, "smp")) {
            args->smp = atoi(arg_val);
            if (args->smp > 1024) {
                rvvm_error("Invalid cores count specified: %s", arg_val);
                return false;
            }
        } else if (cmp_arg(arg_name, "res")) {
            size_t i;
            for (i=0; arg_val[i] && arg_val[i] != 'x'; ++i);
            if (arg_val[i] != 'x') {
                rvvm_error("Invalid resoulution: %s, expects 640x480", arg_val);
                return false;
            }
            args->fb_x = atoi(arg_val);
            args->fb_y = atoi(arg_val + i + 1);
        } else if (cmp_arg(arg_name, "dumpdtb")) {
            args->dumpdtb = arg_val;
        } else if (cmp_arg(arg_name, "rv64")) {
            args->rv64 = true;
            if (argpair == 2) i--;
        } else if (cmp_arg(arg_name, "nogui")) {
            args->nogui = true;
            if (argpair == 2) i--;
        } else if (cmp_arg(arg_name, "help")
                 || cmp_arg(arg_name, "h")
                 || cmp_arg(arg_name, "H")) {
            print_help();
            return false;
        }
    }
    return true;
}

static bool load_file_to_ram(rvvm_machine_t* machine, paddr_t addr, const char* filename)
{
    FILE* file = fopen(filename, "rb");
    size_t fsize;
    uint8_t* buffer;

    if (file == NULL) {
        rvvm_error("Cannot open file %s", filename);
        return false;
    }

    fseek(file, 0, SEEK_END);
    fsize = ftell(file);
    fseek(file, 0, SEEK_SET);

    buffer = safe_malloc(fsize);

    if (fread(buffer, 1, fsize, file) != fsize) {
        rvvm_error("File %s read error", filename);
        fclose(file);
        free(buffer);
        return false;
    }

    if (!rvvm_write_ram(machine, addr, buffer, fsize)) {
        rvvm_error("File %s does not fit in RAM", filename);
        fclose(file);
        free(buffer);
        return false;
    }

    fclose(file);
    free(buffer);
    return true;
}

static bool rvvm_run_with_args(vm_args_t args)
{
    rvvm_machine_t* machine = rvvm_create_machine(RVVM_DEFAULT_MEMBASE, args.mem, args.smp, args.rv64);
    if (machine == NULL) {
        rvvm_error("VM creation failed");
        return false;
    } else if (!load_file_to_ram(machine, machine->mem.begin, args.bootrom)) {
        rvvm_error("Failed to load bootrom");
        return false;
    }

    if (args.dtb) {
        paddr_t dtb_addr = machine->mem.begin + (machine->mem.size >> 1);

        if (!load_file_to_ram(machine, dtb_addr, args.dtb)) {
            rvvm_error("Failed to load DTB");
            return false;
        }

        rvvm_info("Custom DTB loaded at 0x%08"PRIxXLEN, dtb_addr);

        // pass DTB address in a1 register of each hart
        vector_foreach(machine->harts, i) {
            vector_at(machine->harts, i).registers[REGISTER_X11] = dtb_addr;
        }
    }

    if (args.kernel) {
        // Kernel offset is 2MB for RV64, 4MB for RV32 (aka hugepage alignment)

        // TODO: It's possible to move memory region 128k behind and put
        // patched OpenSBI there, to save those precious 4MB
        paddr_t hugepage_offset = args.rv64 ? (2 << 20) : (4 << 20);
        if (!load_file_to_ram(machine, machine->mem.begin + hugepage_offset, args.kernel)) {
            rvvm_error("Failed to load kernel");
            return false;
        }
        rvvm_info("Kernel image loaded at 0x%08"PRIxXLEN, machine->mem.begin + hugepage_offset);
    }

    clint_init(machine, 0x2000000);

    void *plic_data = plic_init(machine, 0xC000000);

    ns16550a_init(machine, 0x10000000, plic_data, 1);
#if defined(USE_FDT) && defined(USE_PCI)
    struct pci_bus_list *pci_buses = pci_bus_init_dt(machine,
		    1, 1, 0x50000000,
		    0x58000000, 0x1000000, 0x59000000, 0x6000000,
		    plic_data, 4);
#endif

    if (args.image) {
        blkdev_t* blk = blk_open(args.image, BLKDEV_RW);
        if (blk == NULL) {
            rvvm_error("Unable to open hard drive image file %s", args.image);
            return false;
        } else {
#if !defined(USE_FDT) || !defined(USE_PCI)
            ata_init(machine, 0x40000000, 0x40001000, blk, NULL);
#else
            ata_init_pci(machine, &pci_buses->buses[0], blk, NULL);
#endif
        }
    }

#ifdef USE_FB
    if (!args.nogui) {
        static struct ps2_device ps2_mouse;
        ps2_mouse = ps2_mouse_create();
        altps2_init(machine, 0x20000000, plic_data, 2, &ps2_mouse);

        static struct ps2_device ps2_keyboard;
        ps2_keyboard = ps2_keyboard_create();
        altps2_init(machine, 0x20001000, plic_data, 3, &ps2_keyboard);

        init_fb(machine, 0x30000000, args.fb_x, args.fb_y, &ps2_mouse, &ps2_keyboard);
    } else {
#else
    {
#endif

#ifdef USE_FDT
        // Broken in FreeBSD for whatever reason
        struct fdt_node* chosen = fdt_node_find(machine->fdt, "chosen");
        if (chosen) fdt_node_add_prop_str(chosen, "stdout-path", "/soc/uart@10000000");
#endif
    }
#ifdef USE_NET
    ethoc_init(machine, 0x21000000, plic_data, 5);
#endif
    syscon_init(machine, 0x100000);
#ifdef USE_RTC
    rtc_goldfish_init(machine, 0x101000, plic_data, 6);
#endif

    if (args.dumpdtb) {
#ifdef USE_FDT
        char buffer[65536];
        size_t size = fdt_serialize(machine->fdt, buffer, sizeof(buffer), 0);
        FILE* file;
        if (size && (file = fopen(args.dumpdtb, "wb"))) {
            fwrite(buffer, size, 1, file);
            fclose(file);
            rvvm_info("DTB dumped to %s, size %u", args.dumpdtb, (uint32_t)size);
        } else {
            rvvm_error("Failed to dump DTB!");
        }
#else
        rvvm_error("This build doesn't support FDT generation");
#endif
    }

    rvvm_enable_builtin_eventloop(false);

    rvvm_start_machine(machine);
    rvvm_run_eventloop(); // Returns on machine shutdown

    bool reset = machine->needs_reset;
    rvvm_free_machine(machine);

/*
 * Example machine resetting code, but a few issues here:
 * - Devices dont't know about resetting at all, may wreak havoc with DMA, etc
 *
 * - We need to reload bootrom & kernel images into ram,
 * but those may be not present after initial boot
 *
 * - Moreover, this should be not done from API user viewpoint but instead wrapped
 * in some helper routine (issue #2 again)
 *
 *
    vector_foreach(machine->harts, i) {
        riscv_tlb_flush(&vector_at(machine->harts, i));
        vector_at(machine->harts, i).registers[REGISTER_PC] = machine->mem.begin;
        vector_at(machine->harts, i).registers[REGISTER_X10] = i;
        vector_at(machine->harts, i).registers[REGISTER_X11] = machine->mem.begin + (machine->mem.size >> 1);
        vector_at(machine->harts, i).priv_mode = PRIVILEGE_MACHINE;
        vector_at(machine->harts, i).pending_events = 0;
    }
    load_file_to_ram(machine, machine->mem.begin, args.bootrom);
    load_file_to_ram(machine, machine->mem.begin + (machine->mem.size >> 1), args.dtb);
    load_file_to_ram(machine, machine->mem.begin + (args.rv64 ? (2 << 20) : (4 << 20)), args.kernel);
*/

    return reset;
}

int main(int argc, const char** argv)
{
    vm_args_t args = {0};
    rvvm_set_args(argc, argv);

    if (!parse_args(argc, argv, &args)) return 0;
    if (args.bootrom == NULL) {
        printf("Usage: %s [-help] [-mem 256M] [-rv64] ... [bootrom]\n", argv[0]);
        return 0;
    }

    while (rvvm_run_with_args(args));
    return 0;
}

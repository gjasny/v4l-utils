// SPDX-License-Identifier: GPL-2.0
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libelf.h>
#include <gelf.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <stdlib.h>
#include <linux/bpf.h>
#include <assert.h>
#include "toml.h"
#include "bpf.h"
#include "bpf_load.h"

#ifdef ENABLE_NLS
# define _(string) gettext(string)
# include "gettext.h"
# include <locale.h>
# include <langinfo.h>
# include <iconv.h>
#else
# define _(string) string
#endif


char bpf_log_buf[BPF_LOG_BUF_SIZE];
extern int debug;

struct bpf_file {
	Elf *elf;
	char license[128];
	bool processed_sec[128];
	int map_fd[MAX_MAPS];
	struct bpf_map_data map_data[MAX_MAPS];
	int nr_maps;
	int maps_shidx;
	int dataidx;
	int bssidx;
	Elf_Data *data;
	int strtabidx;
	Elf_Data *symbols;
	struct toml_table_t *toml;
};

static int load_and_attach(int lirc_fd, struct bpf_file *bpf_file, const char *name, struct bpf_insn *prog, int size)
{
        size_t insns_cnt = size / sizeof(struct bpf_insn);
	int fd, err;

	fd = bpf_load_program(BPF_PROG_TYPE_LIRC_MODE2, prog, insns_cnt,
			      name, bpf_file->license, 0,
			      bpf_log_buf, BPF_LOG_BUF_SIZE);
	if (fd < 0) {
		printf("bpf_load_program() err=%d\n%s", errno, bpf_log_buf);
		return -1;
	}

	err = bpf_prog_attach(fd, lirc_fd, BPF_LIRC_MODE2, 0);
	if (err) {
		printf("bpf_prog_attach: err=%m\n");
		return -1;
	}
	return 0;
}

static int load_maps(struct bpf_file *bpf_file)
{
	struct bpf_map_data *maps = bpf_file->map_data;
	int i, numa_node;

	for (i = 0; i < bpf_file->nr_maps; i++) {
		numa_node = maps[i].def.map_flags & BPF_F_NUMA_NODE ?
			maps[i].def.numa_node : -1;

		if (maps[i].def.type == BPF_MAP_TYPE_ARRAY_OF_MAPS ||
		    maps[i].def.type == BPF_MAP_TYPE_HASH_OF_MAPS) {
			int inner_map_fd = bpf_file->map_fd[maps[i].def.inner_map_idx];

			bpf_file->map_fd[i] = bpf_create_map_in_map_node(
							maps[i].def.type,
							maps[i].name,
							maps[i].def.key_size,
							inner_map_fd,
							maps[i].def.max_entries,
							maps[i].def.map_flags,
							numa_node);
		} else {
			bpf_file->map_fd[i] = bpf_create_map_node(
							maps[i].def.type,
							maps[i].name,
							maps[i].def.key_size,
							maps[i].def.value_size,
							maps[i].def.max_entries,
							maps[i].def.map_flags,
							numa_node);
		}
		if (bpf_file->map_fd[i] < 0) {
			printf(_("failed to create a map: %d %s\n"),
			       errno, strerror(errno));
			return 1;
		}
		maps[i].fd = bpf_file->map_fd[i];
	}
	return 0;
}

static int get_sec(Elf *elf, int i, GElf_Ehdr *ehdr, char **shname,
		   GElf_Shdr *shdr, Elf_Data **data)
{
	Elf_Scn *scn;

	scn = elf_getscn(elf, i);
	if (!scn)
		return 1;

	if (gelf_getshdr(scn, shdr) != shdr)
		return 2;

	*shname = elf_strptr(elf, ehdr->e_shstrndx, shdr->sh_name);
	if (!*shname || !shdr->sh_size)
		return 3;

	*data = elf_getdata(scn, 0);
	if (!*data || elf_getdata(scn, *data) != NULL)
		return 4;

	return 0;
}

static int parse_relo_and_apply(struct bpf_file *bpf_file, GElf_Shdr *shdr,
				struct bpf_insn *insn, Elf_Data *data)
{
	int i, nrels;

	nrels = shdr->sh_size / shdr->sh_entsize;

	for (i = 0; i < nrels; i++) {
		GElf_Sym sym;
		GElf_Rel rel;
		unsigned int insn_idx;
		const char *sym_name;
		bool match = false;
		int map_idx;

		gelf_getrel(data, i, &rel);

		insn_idx = rel.r_offset / sizeof(struct bpf_insn);

		gelf_getsym(bpf_file->symbols, GELF_R_SYM(rel.r_info), &sym);

		sym_name = elf_strptr(bpf_file->elf, bpf_file->strtabidx, sym.st_name);

		if (insn[insn_idx].code != (BPF_LD | BPF_IMM | BPF_DW)) {
			printf(_("invalid relo for insn[%d].code 0x%x\n"),
			       insn_idx, insn[insn_idx].code);
			return 1;
		}

		if (sym.st_shndx == bpf_file->maps_shidx) {
			/* Match FD relocation against recorded map_data[] offset */
			for (map_idx = 0; map_idx < bpf_file->nr_maps; map_idx++) {
				if (bpf_file->map_data[map_idx].elf_offset == sym.st_value) {
					match = true;
					break;
				}
			}

			if (match) {
		                insn[insn_idx].src_reg = BPF_PSEUDO_MAP_FD;
				insn[insn_idx].imm = bpf_file->map_data[map_idx].fd;
				continue;
			}

			printf(_("invalid relo for insn[%d] no map_data match\n"),
			       insn_idx);
			return 1;
		}
		else if (sym.st_shndx == bpf_file->dataidx || sym.st_shndx == bpf_file->bssidx) {
			const char *raw = NULL;
			int value = 0;

			if (!bpf_param(sym_name, &value)) {
				// done
			} else if (bpf_file->toml &&
				   (raw = toml_raw_in(bpf_file->toml, sym_name)) != NULL) {
				int64_t val64;

				if (toml_rtoi(raw, &val64)) {
					printf(_("variable %s not a integer: %s\n"), sym_name, raw);
					return 1;
				}

				if (value < INT_MIN && value > UINT_MAX) {
					printf(_("variable %s out of range: %s\n"), sym_name, raw);
					return 1;
				}

				value = val64;
			} else if (sym.st_shndx == bpf_file->dataidx) {
				value = *(int*)((unsigned char*)bpf_file->data->d_buf + sym.st_value);
			}

			if (debug)
				printf(_("patching insn[%d] with immediate %d for symbol %s\n"), insn_idx, value, sym_name);

			// patch ld to mov immediate
			insn[insn_idx].imm = value;
		} else {
			printf(_("symbol %s has unknown section %d\n"), sym_name, sym.st_shndx);
			return 1;
		}
	}

	return 0;
}

static int cmp_symbols(const void *l, const void *r)
{
	const GElf_Sym *lsym = (const GElf_Sym *)l;
	const GElf_Sym *rsym = (const GElf_Sym *)r;

	if (lsym->st_value < rsym->st_value)
		return -1;
	else if (lsym->st_value > rsym->st_value)
		return 1;
	else
		return 0;
}

static int load_elf_maps_section(struct bpf_file *bpf_file)
{
	int map_sz_elf, map_sz_copy;
	bool validate_zero = false;
	Elf_Data *data_maps;
	int i, nr_maps;
	GElf_Sym *sym;
	Elf_Scn *scn;

	if (bpf_file->maps_shidx < 0)
		return -EINVAL;
	if (!bpf_file->symbols)
		return -EINVAL;

	/* Get data for maps section via elf index */
	scn = elf_getscn(bpf_file->elf, bpf_file->maps_shidx);
	if (scn)
		data_maps = elf_getdata(scn, NULL);
	if (!scn || !data_maps) {
		printf(_("Failed to get Elf_Data from maps section %d\n"),
		       bpf_file->maps_shidx);
		return -EINVAL;
	}

	/* For each map get corrosponding symbol table entry */
	sym = calloc(MAX_MAPS+1, sizeof(GElf_Sym));
	for (i = 0, nr_maps = 0; i < bpf_file->symbols->d_size / sizeof(GElf_Sym); i++) {
		assert(nr_maps < MAX_MAPS+1);
		if (!gelf_getsym(bpf_file->symbols, i, &sym[nr_maps]))
			continue;
		if (sym[nr_maps].st_shndx != bpf_file->maps_shidx)
			continue;
		/* Only increment iif maps section */
		nr_maps++;
	}

	/* Align to map_fd[] order, via sort on offset in sym.st_value */
	qsort(sym, nr_maps, sizeof(GElf_Sym), cmp_symbols);

	/* Keeping compatible with ELF maps section changes
	 * ------------------------------------------------
	 * The program size of struct bpf_load_map_def is known by loader
	 * code, but struct stored in ELF file can be different.
	 *
	 * Unfortunately sym[i].st_size is zero.  To calculate the
	 * struct size stored in the ELF file, assume all struct have
	 * the same size, and simply divide with number of map
	 * symbols.
	 */
	map_sz_elf = data_maps->d_size / nr_maps;
	map_sz_copy = sizeof(struct bpf_load_map_def);
	if (map_sz_elf < map_sz_copy) {
		/*
		 * Backward compat, loading older ELF file with
		 * smaller struct, keeping remaining bytes zero.
		 */
		map_sz_copy = map_sz_elf;
	} else if (map_sz_elf > map_sz_copy) {
		/*
		 * Forward compat, loading newer ELF file with larger
		 * struct with unknown features. Assume zero means
		 * feature not used.  Thus, validate rest of struct
		 * data is zero.
		 */
		validate_zero = true;
	}

	/* Memcpy relevant part of ELF maps data to loader maps */
	for (i = 0; i < nr_maps; i++) {
		unsigned char *addr, *end, *def;
		const char *map_name;
		struct bpf_map_data *maps = bpf_file->map_data;
		size_t offset;

		map_name = elf_strptr(bpf_file->elf, bpf_file->strtabidx, sym[i].st_name);
		maps[i].name = strdup(map_name);
		if (!maps[i].name) {
			printf(_("strdup(%s): %s(%d)\n"), map_name,
			       strerror(errno), errno);
			free(sym);
			return -errno;
		}

		/* Symbol value is offset into ELF maps section data area */
		offset = sym[i].st_value;
		def = (unsigned char*)data_maps->d_buf + offset;
		maps[i].elf_offset = offset;
		memset(&maps[i].def, 0, sizeof(struct bpf_load_map_def));
		memcpy(&maps[i].def, def, map_sz_copy);

		/* Verify no newer features were requested */
		if (validate_zero) {
			addr = def + map_sz_copy;
			end  = def + map_sz_elf;
			for (; addr < end; addr++) {
				if (*addr != 0) {
					free(sym);
					return -EFBIG;
				}
			}
		}
	}

	free(sym);
	return nr_maps;
}

int load_bpf_file(const char *path, int lirc_fd, struct toml_table_t *toml)
{
	struct bpf_file bpf_file = { .toml = toml };
	int fd, i, ret;
	Elf *elf;
	GElf_Ehdr ehdr;
	GElf_Shdr shdr, shdr_prog;
	Elf_Data *data, *data_prog, *data_map = NULL;
	char *shname, *shname_prog;
	int nr_maps = 0;

	if (elf_version(EV_CURRENT) == EV_NONE)
		return 1;

	fd = open(path, O_RDONLY, 0);
	if (fd < 0)
		return 1;

	elf = elf_begin(fd, ELF_C_READ, NULL);

	if (!elf)
		return 1;

	if (gelf_getehdr(elf, &ehdr) != &ehdr)
		return 1;

	bpf_file.elf = elf;

	/* scan over all elf sections to get license and map info */
	for (i = 1; i < ehdr.e_shnum; i++) {

		if (get_sec(elf, i, &ehdr, &shname, &shdr, &data))
			continue;

		if (debug)
			printf(_("section %d:%s data %p size %zd link %d flags %d\n"),
			       i, shname, data->d_buf, data->d_size,
			       shdr.sh_link, (int) shdr.sh_flags);

		if (strcmp(shname, "license") == 0) {
			bpf_file.processed_sec[i] = true;
			memcpy(bpf_file.license, data->d_buf, data->d_size);
		} else if (strcmp(shname, "maps") == 0) {
			int j;

			bpf_file.maps_shidx = i;
			data_map = data;
			for (j = 0; j < MAX_MAPS; j++)
				bpf_file.map_data[j].fd = -1;
		} else if (strcmp(shname, ".data") == 0) {
			bpf_file.dataidx = i;
			bpf_file.data = data;
		} else if (strcmp(shname, ".bss") == 0) {
			bpf_file.bssidx = i;
		} else if (shdr.sh_type == SHT_SYMTAB) {
			bpf_file.strtabidx = shdr.sh_link;
			bpf_file.symbols = data;
		}
	}

	ret = 1;

	if (!bpf_file.symbols) {
		printf(_("missing SHT_SYMTAB section\n"));
		goto done;
	}

	if (data_map) {
		bpf_file.nr_maps = load_elf_maps_section(&bpf_file);
		if (bpf_file.nr_maps < 0) {
			printf(_("Error: Failed loading ELF maps (errno:%d):%s\n"),
			       nr_maps, strerror(-nr_maps));
			goto done;
		}
		if (load_maps(&bpf_file))
			goto done;

		bpf_file.processed_sec[bpf_file.maps_shidx] = true;
	}

	/* process all relo sections, and rewrite bpf insns for maps */
	for (i = 1; i < ehdr.e_shnum; i++) {
		if (bpf_file.processed_sec[i])
			continue;

		if (get_sec(elf, i, &ehdr, &shname, &shdr, &data))
			continue;

		if (shdr.sh_type == SHT_REL) {
			struct bpf_insn *insns;

			/* locate prog sec that need map fixup (relocations) */
			if (get_sec(elf, shdr.sh_info, &ehdr, &shname_prog,
				    &shdr_prog, &data_prog))
				continue;

			if (shdr_prog.sh_type != SHT_PROGBITS ||
			    !(shdr_prog.sh_flags & SHF_EXECINSTR))
				continue;

			insns = (struct bpf_insn *) data_prog->d_buf;
			bpf_file.processed_sec[i] = true; /* relo section */

			if (parse_relo_and_apply(&bpf_file, &shdr, insns, data))
				continue;
		}
	}

	/* load programs */
	for (i = 1; i < ehdr.e_shnum; i++) {
		if (bpf_file.processed_sec[i])
			continue;

		if (get_sec(elf, i, &ehdr, &shname, &shdr, &data))
			continue;

		if (shdr.sh_type != SHT_PROGBITS ||
		    !(shdr.sh_flags & SHF_EXECINSTR))
			continue;

		ret = load_and_attach(lirc_fd, &bpf_file, shname, data->d_buf,
				      data->d_size);
		break;
	}

done:
	close(fd);
	return ret;
}

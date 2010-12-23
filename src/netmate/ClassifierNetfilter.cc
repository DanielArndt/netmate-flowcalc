
/*!\file   ClassifierNetfilter.cc

    Copyright 2003-2004 Fraunhofer Institute for Open Communication Systems (FOKUS),
                        Berlin, Germany

    This file is part of Network Measurement and Accounting System (NETMATE).

    NETMATE is free software; you can redistribute it and/or modify 
    it under the terms of the GNU General Public License as published by 
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    NETMATE is distributed in the hope that it will be useful, 
    but WITHOUT ANY WARRANTY; without even the implied warranty of 
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this software; if not, write to the Free Software 
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Description:
    implementation for ClassifierNetfilter class

    $Id: ClassifierNetfilter.cc 748 2009-09-10 02:54:03Z szander $
*/

#include "ClassifierNetfilter.h"

/* defines section mostly from iptables.c */

#define MYBUFSIZ 256000
#ifndef IPT_LIB_DIR
#define IPT_LIB_DIR "/usr/local/lib/iptables"
#endif
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef PROC_SYS_MODPROBE
#define PROC_SYS_MODPROBE "/proc/sys/kernel/modprobe"
#endif

#define FMT_NUMERIC	      0x0001
#define FMT_NOCOUNTS	  0x0002
#define FMT_KILOMEGAGIGA  0x0004
#define FMT_OPTIONS	      0x0008
#define FMT_NOTABLE	      0x0010
#define FMT_NOTARGET	  0x0020
#define FMT_VIA		      0x0040
#define FMT_NONEWLINE	  0x0080
#define FMT_LINENUMBERS   0x0100

#define FMT_PRINT_RULE (FMT_NOCOUNTS | FMT_OPTIONS | FMT_VIA | FMT_NUMERIC | FMT_NOTABLE)
#define FMT(tab,notab) ((format) & FMT_NOTABLE ? (notab) : (tab))


// default values

const char *def_tablename  = "meter";
const char *def_chainname  = "meter_base";
const char *def_targetname = "ULOG";
const int def_nlgroup      = 4;
const int def_cprange      = 100;
const int def_rtqt         = 1;
const int def_nonrtqt      = 50; //! limited to 50, see ipt_ulog.c
const char *def_modprobe   = "modprobe";
const char *inv_symbol     = "!";
//const char *program_name = "iptables";

/* globals, dont use static because they are extern in iptables.h */

struct iptables_match  *iptables_matches = NULL;
struct iptables_target *iptables_targets = NULL;

#define OPTION_OFFSET 256
static struct option original_opts[] = {
    { 0 }
};
static struct option *opts = original_opts;
static unsigned int global_option_offset = 0;

/* default iptables and ULOG parameters */

enum def_parameters {
    defp_srcipmask,
    defp_dstipmask,
    defp_prot,
    defp_iniface,
    defp_outiface,
    defp_nlgroup,
    defp_cprange,
    defp_prefix,
    defp_qthreshold,
    defp_match,
    defp_bidir
};

static const char *param_names[] = {
    { "srcip" },
    { "dstip" },
    { "proto" },
    { "iiface" },
    { "oiface" },
    { "ulog-nlgroup" },
    { "ulog-cprange" },
    { "ulog-prefix" },
    { "ulog-qthreshold" },
    { "match" },
    { "bidir" },
    { 0 }
};

/* which basic params are allowed inverse */

static int inverse_for_params[] = 
  {
      /* srpip */ IPT_INV_SRCIP,
      /* dstip */ IPT_INV_DSTIP,
      /* proto */ IPT_INV_PROTO,
      /* iiface */ IPT_INV_VIA_IN,
      /* oiface */ IPT_INV_VIA_OUT,
      0,
      0,
      0,
      0,
      0,
  };

/* mapping table, map mcl parameter to netfilter modules paramter */

struct opt_map {
    char *mcl;
    char *iptables;
};

static const struct opt_map option_map[] = {
    { "srcport", "sport" },
    { "dstport", "dport" },
    { "snaplen", "ulog-cprange" },
    { "qthresh", "ulog-qthreshold" },
    { 0 }
};

/* A few hardcoded protocols for 'all' and in case the user has no
   /etc/protocols */
struct pprot {
    char *name;
    u_int8_t num;
};

static const struct pprot chain_protos[] = {
    { "tcp", IPPROTO_TCP },
    { "udp", IPPROTO_UDP },
    { "icmp", IPPROTO_ICMP },
    { "esp", IPPROTO_ESP },
    { "ah", IPPROTO_AH },
    { "all", 0 },
};


/* ------------------- memory alloc helper functions ------------------ */

// FIXME: use new and delete
// FIXME: check for mem leaks

static void *fw_calloc(size_t count, size_t size)
{
    void *p;

    if ((p = calloc(count, size)) == NULL) {
	throw MyErr("calloc failed: %s", strerror(errno));
    }
    return p;
}

static void *fw_malloc(size_t size)
{
    void *p;

    if ((p = malloc(size)) == NULL) {
	throw MyErr("malloc failed: %s", strerror(errno));
    }
    return p;
}

/* -------------- iptables functions called by modules -------------------- */

/*static*/ struct option *merge_options(struct option *oldopts, const struct option *newopts,
					unsigned int *option_offset)
{
    unsigned int num_old, num_new, i;
    struct option *merge;

    for (num_old = 0; oldopts[num_old].name; num_old++);
    for (num_new = 0; newopts[num_new].name; num_new++);

    global_option_offset += OPTION_OFFSET;
    *option_offset = global_option_offset;

    merge = (struct option *) fw_malloc(sizeof(struct option) * (num_new + num_old + 1));
    memcpy(merge, oldopts, num_old * sizeof(struct option));
    for (i = 0; i < num_new; i++) {
	merge[num_old + i] = newopts[i];
	merge[num_old + i].val += *option_offset;
    }
    memset(merge + num_old + num_new, 0, sizeof(struct option));

    return merge;
}

extern "C" {
void exit_error(enum exittype status, char *msg, ...)
{
    va_list args;
    char err_msg[1024];

    va_start(args, msg);
    vsprintf(err_msg, msg, args);
    va_end(args);
	
    if (status == PARAMETER_PROBLEM)
	strcat(err_msg, "\nRead the manual for more information.\n");
    if (status == VERSION_PROBLEM)
	strcat(err_msg, "\nPerhaps netmate or your kernel needs to be upgraded.\n");
   
    // FIXME: currently exceptions are not catched if thrown from within a C module
    fprintf(stderr, "%s", err_msg);
    throw MyErr(err_msg);
}

int string_to_number(const char *s, unsigned int min, unsigned int max,
		     unsigned int *ret)
{
    long number;
    char *end;
  
    /* Handle hex, octal, etc. */
    errno = 0;
    number = strtol(s, &end, 0);
    if ((*end == '\0') && (end != s)) {
	/* we parsed a number, let's see if we want this */
	if ((errno != ERANGE) && ((long)min <= number) && (number <= (long)max)) {
	    *ret = number;
	    return 0;
	}
    }
    return -1;
}              

int check_inverse(const char option[], int *invert, int *optind, int argc)
{
    if (option && strcmp(option, "!") == 0) {
	if (*invert)
	    exit_error(PARAMETER_PROBLEM,
		       "Multiple `!' flags not allowed");
	*invert = TRUE;
	if (optind) {
	    *optind = *optind+1;
	    if (argc && *optind > argc)
		exit_error(PARAMETER_PROBLEM,
			   "no argument following `!'");
	}

	return TRUE;
    }
    return FALSE;
}
/*int check_inverse(const char option[], int *invert)
  {
  if (option && strcmp(option, "!") == 0) {
  if (*invert)
  exit_error(PARAMETER_PROBLEM,
  "Multiple `!' flags not allowed");

  *invert = TRUE;
  return TRUE;
  }
  return FALSE;
  }*/

void register_match(struct iptables_match *me)
{
    struct iptables_match **i;

    if (strcmp(me->version, PROGRAM_VERSION) != 0) {
	exit_error(VERSION_PROBLEM, "match `%s' v%s (I'm v%s).\n",
		   me->name, me->version, PROGRAM_VERSION);
    }

    if (find_match(me->name, DONT_LOAD)) {
	exit_error(PARAMETER_PROBLEM, "match `%s' already registered.\n",
		   me->name);
    }

    if (me->size != IPT_ALIGN(me->size)) {
	exit_error(PARAMETER_PROBLEM, "match `%s' has invalid size %u.\n",
		   me->name, me->size);
    }

    /* Append to list. */
    for (i = &iptables_matches; *i; i = &(*i)->next);
    me->next = NULL;
    *i = me;

    me->m = NULL;
    me->mflags = 0;

    opts = merge_options(opts, me->extra_opts, &me->option_offset);
}

void register_target(struct iptables_target *me)
{

    if (strcmp(me->version, PROGRAM_VERSION) != 0) {
	exit_error(VERSION_PROBLEM, "target `%s' v%s (I'm v%s).\n",
		   me->name, me->version, PROGRAM_VERSION);
	   
    }

    if (find_target(me->name, DONT_LOAD)) {
	exit_error(PARAMETER_PROBLEM, "target `%s' already registered.\n",
		   me->name);
    }

    if (me->size != IPT_ALIGN(me->size)) {
	exit_error(PARAMETER_PROBLEM, "target `%s' has invalid size %u.\n",
		   me->name, me->size);
    }

    /* Prepend to list. */
    me->next = iptables_targets;
    iptables_targets = me;
    me->t = NULL;
    me->tflags = 0;

    opts = merge_options(opts, me->extra_opts, &me->option_offset);
}

}

/* ---------------------- iptables helper functions ----------------------- */

/* translate module options from mcl to iptables */
/*static*/ int translate_option(char *rule_param, int offset)
{
    int num = 0;

    num = 0;
    while (option_map[num].mcl) {
	if (!strcmp(option_map[num].mcl, rule_param))
	    rule_param = option_map[num].iptables;
 
	num++;
    }
		 
    num = 0;
    while (opts[num].name) {
	if ((opts[num].val >= offset) && (opts[num].val < offset+OPTION_OFFSET) &&
	    !strcmp(opts[num].name, rule_param))
	    return opts[num].val;

	num++;
    }

    return -1;
}

/*static*/ void inaddrcpy(struct in_addr *dst, struct in_addr *src)
{
    /* memcpy(dst, src, sizeof(struct in_addr)); */
    dst->s_addr = src->s_addr;
}

/*static*/ struct in_addr *network_to_addr(const char *name)
{
    struct netent *net;
    static struct in_addr addr;

    if ((net = getnetbyname(name)) != NULL) {
	if (net->n_addrtype != AF_INET)
	    return (struct in_addr *) NULL;
	addr.s_addr = htonl((unsigned long) net->n_net);
	return &addr;
    }

    return (struct in_addr *) NULL;
}

/*static*/ struct in_addr *host_to_addr(const char *name, unsigned int *naddr)
{
    struct hostent *host;
    struct in_addr *addr;
    unsigned int i;

    *naddr = 0;
    if ((host = gethostbyname(name)) != NULL) {
	if (host->h_addrtype != AF_INET ||
	    host->h_length != sizeof(struct in_addr))
	    return (struct in_addr *) NULL;

	while (host->h_addr_list[*naddr] != (char *) NULL)
	    (*naddr)++;
	addr = (struct in_addr *) fw_calloc(*naddr, sizeof(struct in_addr));
	for (i = 0; i < *naddr; i++)
	    inaddrcpy(&(addr[i]),
		      (struct in_addr *) host->h_addr_list[i]);
	return addr;
    }

    return (struct in_addr *) NULL;
}

/*static*/ struct in_addr *dotted_to_addr(const char *dotted)
{
    static struct in_addr addr;
    unsigned char *addrp;
    char *p, *q;
    unsigned int onebyte;
    int i;
    char buf[20];
 
    /* copy dotted string, because we need to modify it */
    strncpy(buf, dotted, sizeof(buf) - 1);
    addrp = (unsigned char *) &(addr.s_addr);
 
    p = buf;
    for (i = 0; i < 3; i++) {
	if ((q = strchr(p, '.')) == NULL)
	    return (struct in_addr *) NULL;
	
	*q = '\0';
	if (string_to_number(p, 0, 255, &onebyte) == -1)
	    return (struct in_addr *) NULL;
	
	addrp[i] = (unsigned char) onebyte;
	p = q + 1;
    }
  
    /* we've checked 3 bytes, now we check the last one */
    if (string_to_number(p, 0, 255, &onebyte) == -1)
	return (struct in_addr *) NULL;
  
    addrp[3] = (unsigned char) onebyte;
  
    return &addr;
}                          

#ifdef CLASS_DEBUG

/*static*/ char *addr_to_host(const struct in_addr *addr)
{
    struct hostent *host;

    if ((host = gethostbyaddr((char *) addr,
			      sizeof(struct in_addr), AF_INET)) != NULL)
	return (char *) host->h_name;

    return (char *) NULL;
}

/*static*/ char *addr_to_network(const struct in_addr *addr)
{
    struct netent *net;

    if ((net = getnetbyaddr((long) ntohl(addr->s_addr), AF_INET)) != NULL)
	return (char *) net->n_name;

    return (char *) NULL;
}

/*static*/ char *addr_to_dotted(const struct in_addr *addrp)
{
    static char buf[20];
    const unsigned char *bytep;

    bytep = (const unsigned char *) &(addrp->s_addr);
    sprintf(buf, "%d.%d.%d.%d", bytep[0], bytep[1], bytep[2], bytep[3]);
    return buf;
}

/*static*/ char *mask_to_dotted(const struct in_addr *mask)
{
    int i;
    static char buf[20];
    u_int32_t maskaddr, bits;

    maskaddr = ntohl(mask->s_addr);

    if (maskaddr == 0xFFFFFFFFL)
	/* we don't want to see "/32" */
	return "";

    i = 32;
    bits = 0xFFFFFFFEL;
    while (--i >= 0 && maskaddr != bits)
	bits < <= 1;
    if (i >= 0)
	sprintf(buf, "/%d", i);
    else
	/* mask was not a decent combination of 1's and 0's */
	sprintf(buf, "/%s", addr_to_dotted(mask));

    return buf;
}

/*static*/ char *addr_to_anyname(const struct in_addr *addr)
{
    char *name;

    if ((name = addr_to_host(addr)) != NULL ||
	(name = addr_to_network(addr)) != NULL)
	return name;

    return addr_to_dotted(addr);
}

#endif

/*static*/ char *proto_to_name(u_int8_t proto, int nolookup)
{
    unsigned int i;

    if (proto && !nolookup) {
	struct protoent *pent = getprotobynumber(proto);
	if (pent)
	    return pent->p_name;
    }

    for (i = 0; i < sizeof(chain_protos)/sizeof(struct pprot); i++)
	if (chain_protos[i].num == proto)
	    return chain_protos[i].name;

    return NULL;
}

/*static*/ u_int16_t parse_protocol(const char *s)
{
    unsigned int proto;
  
    if (string_to_number(s, 0, 255, &proto) == -1) {
	struct protoent *pent;
 
	if ((pent = getprotobyname(s)))
	    proto = pent->p_proto;
	else {
	    unsigned int i;
	    for (i = 0; i < sizeof(chain_protos)/sizeof(struct pprot); i++) {
		if (strcmp(s, chain_protos[i].name) == 0) {
		    proto = chain_protos[i].num;
		    break;
		}
	    }
	    if (i == sizeof(chain_protos)/sizeof(struct pprot))
		exit_error(PARAMETER_PROBLEM, "unknown protocol `%s' specified", s);
	}
    }
  
    return (u_int16_t)proto;
}                           

/*static*/ struct in_addr *parse_hostnetwork(const char *name, unsigned int *naddrs)
{
    struct in_addr *addrp, *addrptmp;

    if ((addrptmp = dotted_to_addr(name)) != NULL ||
	(addrptmp = network_to_addr(name)) != NULL) {
	addrp = (struct in_addr *) fw_malloc(sizeof(struct in_addr));
	inaddrcpy(addrp, addrptmp);
	*naddrs = 1;
	return addrp;
    }
    if ((addrp = host_to_addr(name, naddrs)) != NULL)
	return addrp;
	
    exit_error(PARAMETER_PROBLEM, "host/network `%s' not found", name);
}

/*static*/ struct in_addr *parse_mask(char *mask)
{
    static struct in_addr maskaddr;
    struct in_addr *addrp;
    unsigned int bits;
		
    if (mask == NULL) {
	/* no mask at all defaults to 32 bits */
	maskaddr.s_addr = 0xFFFFFFFF;
	return &maskaddr;
    }
    if ((addrp = dotted_to_addr(mask)) != NULL)
	/* dotted_to_addr already returns a network byte order addr */
	return addrp;
    if (string_to_number(mask, 0, 32, &bits) == -1)
	exit_error(PARAMETER_PROBLEM,
		   "invalid mask `%s' specified", mask);
    if (bits != 0) {
	maskaddr.s_addr = htonl(0xFFFFFFFF << (32 - bits));
	return &maskaddr;
    }
  
    maskaddr.s_addr = 0L;
    return &maskaddr;
}    

/*static*/ void parse_hostnetworkmask(const char *name, struct in_addr **addrpp,
				      struct in_addr *maskp, unsigned int *naddrs)
{
    struct in_addr *addrp;
    char buf[256];
    char *p;
    int i, j, k, n;

    strncpy(buf, name, sizeof(buf) - 1);
    if ((p = strrchr(buf, '/')) != NULL) {
	*p = '\0';
	addrp = parse_mask(p + 1);
    } else
	addrp = parse_mask(NULL);
    inaddrcpy(maskp, addrp);

    /* if a null mask is given, the name is ignored, like in "any/0" */
    if (maskp->s_addr == 0L)
	strcpy(buf, "0.0.0.0");

    addrp = *addrpp = parse_hostnetwork(buf, naddrs);
    n = *naddrs;
    for (i = 0, j = 0; i < n; i++) {
	addrp[j++].s_addr & = maskp->s_addr;
	for (k = 0; k < j - 1; k++) {
	    if (addrp[k].s_addr == addrp[j - 1].s_addr) {
		(*naddrs)--;
		j--;
		break;
	    }
	}
    }
}

/*static*/ const char *parse_target(const char *targetname)
{
    const char *ptr;

    if (strlen(targetname) < 1)
	exit_error(PARAMETER_PROBLEM, "Invalid target name (too short)");

    if (strlen(targetname)+1 > sizeof(ipt_chainlabel))
	exit_error(PARAMETER_PROBLEM, "Invalid target name `%s' (%i chars max)",
		   targetname, sizeof(ipt_chainlabel)-1);

    for (ptr = targetname; *ptr; ptr++)
	if (isspace(*ptr))
	    exit_error(PARAMETER_PROBLEM,"Invalid target name `%s'", targetname);

    return targetname;
}

/*static*/ void parse_interface(const char *arg, char *vianame, unsigned char *mask)
{
    int vialen = strlen(arg);
    unsigned int i;

    memset(mask, 0, IFNAMSIZ);
    memset(vianame, 0, IFNAMSIZ);

    if (vialen + 1 > IFNAMSIZ)
	exit_error(PARAMETER_PROBLEM,
		   "interface name `%s' must be shorter than IFNAMSIZ"
		   " (%i)", arg, IFNAMSIZ-1);

    strcpy(vianame, arg);
    if (vialen == 0)
	memset(mask, 0, IFNAMSIZ);
    else if (vianame[vialen - 1] == '+') {
	memset(mask, 0xFF, vialen - 1);
	memset(mask + vialen - 1, 0, IFNAMSIZ - vialen + 1);
	/* Remove `+' */
	vianame[vialen - 1] = '\0';
    } else {
	/* Include nul-terminator in match */
	memset(mask, 0xFF, vialen + 1);
	memset(mask + vialen + 1, 0, IFNAMSIZ - vialen - 1);
    }
    for (i = 0; vianame[i]; i++) {
	if (!isalnum(vianame[i])) {
	    printf("Warning: wierd character in interface"
		   " `%s' (No aliases, :, !or *).\n",
		   vianame);
	    break;
	}
    }
}

/*static*/ struct iptables_match *find_match(const char *name, enum ipt_tryload tryload)
{
    struct iptables_match *ptr;

    for (ptr = iptables_matches; ptr; ptr = ptr->next) {
	if (strcmp(name, ptr->name) == 0)
	    break;
    }

    if (!ptr && tryload != DONT_LOAD) {
	char path[sizeof(IPT_LIB_DIR) + sizeof("/libipt_.so")
		  + strlen(name)];
	sprintf(path, IPT_LIB_DIR "/libipt_%s.so", name);
	if (dlopen(path, RTLD_NOW)) {
	    /* Found library.  If it didn't register itself,
	       maybe they specified target as match. */
	    ptr = find_match(name, DONT_LOAD);

	    if (!ptr)
		exit_error(PARAMETER_PROBLEM, "Couldn't load match `%s'\n", name);
	} else if (tryload == LOAD_MUST_SUCCEED)
	    exit_error(PARAMETER_PROBLEM, "Couldn't load match `%s':%s\n", name, dlerror());
    }

    if (ptr)
	ptr->used = 1;

    return ptr;
}

/*static*/ struct iptables_match *find_proto(const char *pname, enum ipt_tryload tryload, 
					     int nolookup)
{
    unsigned int proto;
  
    if (string_to_number(pname, 0, 255, &proto) != -1)
	return find_match(proto_to_name(proto, nolookup), tryload);
  
    return find_match(pname, tryload);
}      

/*static*/ struct iptables_target *find_target(const char *name, enum ipt_tryload tryload)
{
    struct iptables_target *ptr;

    /* Standard target? */
    if (strcmp(name, "") == 0
	|| strcmp(name, IPTC_LABEL_ACCEPT) == 0
	|| strcmp(name, IPTC_LABEL_DROP) == 0
	|| strcmp(name, IPTC_LABEL_QUEUE) == 0
	|| strcmp(name, IPTC_LABEL_RETURN) == 0)
	name = "standard";

    for (ptr = iptables_targets; ptr; ptr = ptr->next) {
	if (strcmp(name, ptr->name) == 0)
	    break;
    }

    if (!ptr && tryload != DONT_LOAD) {
	char path[sizeof(IPT_LIB_DIR) + sizeof("/libipt_.so")
		  + strlen(name)];
	sprintf(path, IPT_LIB_DIR "/libipt_%s.so", name);
   
	if (dlopen(path, RTLD_NOW)) {

	    /* Found library.  If it didn't register itself,
	       maybe they specified match as a target. */		 
	    ptr = find_target(name, DONT_LOAD);
	    if (!ptr)
		exit_error(PARAMETER_PROBLEM, "Couldn't load target `%s'\n", name);
	} else if (tryload == LOAD_MUST_SUCCEED)
	    exit_error(PARAMETER_PROBLEM, "Couldn't load target `%s':%s\n", name, dlerror());
    }

    if (ptr)
	ptr->used = 1;

    return ptr;
}

/*static*/ struct ipt_entry *generate_entry(const struct ipt_entry *fw,
					    struct iptables_match *matches,
					    struct ipt_entry_target *target)
{
    unsigned int size;
    struct iptables_match *m;
    struct ipt_entry *e;

    size = sizeof(struct ipt_entry);
    for (m = matches; m; m = m->next) {
	if (m->used) {
	    size += m->m->u.match_size;
	}
    }
		
    e = (struct ipt_entry *) fw_malloc(size + target->u.target_size);

    *e = *fw;
    e->target_offset = size;

    e->next_offset = size + target->u.target_size;

    size = 0;
    for (m = matches; m; m = m->next) {
	if (m->used) {
	    memcpy(e->elems + size, m->m, m->m->u.match_size);
	    size += m->m->u.match_size;
	}
    }
    memcpy(e->elems + size, target, target->u.target_size);

    return e;
}

/*static*/ char *get_modprobe(void)
{
    int procfile;
    char *ret;

    procfile = open(PROC_SYS_MODPROBE, O_RDONLY);
    if (procfile < 0)
	return NULL;

    ret = (char *) fw_malloc(1024);
    if (ret) {
	switch (read(procfile, ret, 1024)) {
	case -1: goto fail;
	case 1024: goto fail; /* Partial read.  Wierd */
	}
	return ret;
    }
  fail:
    free(ret);
    close(procfile);
    return NULL;
}

/*static*/ int iptables_insmod(const char *modname, const char *modprobe)
{
    char *buf = NULL;
    char *argv[3];

    /* If they don't explicitly set it, read out of kernel */
    if (!modprobe) {
	buf = get_modprobe();
	if (!buf)
	    return -1;
	modprobe = buf;
    }

    switch (fork()) {
    case 0:
	argv[0] = (char *)modprobe;
	argv[1] = (char *)modname;
	argv[2] = NULL;
	execv(argv[0], argv);

	/* not usually reached */
	exit(0);
    case -1:
	return -1;

    default: /* parent */
	wait(NULL);
    }

    free(buf);
    return 0;
}

#ifdef CLASS_DEBUG

/*static*/ void print_num(u_int64_t number, unsigned int format)
{
    if (format & FMT_KILOMEGAGIGA) {
	if (number > 99999) {
	    number = (number + 500) / 1000;
	    if (number > 9999) {
		number = (number + 500) / 1000;
		if (number > 9999) {
		    number = (number + 500) / 1000;
		    printf(FMT("%4lluG ","%lluG "),number);
		}
		else printf(FMT("%4lluM ","%lluM "), number);
	    } else
		printf(FMT("%4lluK ","%lluK "), number);
	} else
	    printf(FMT("%5llu ","%llu "), number);
    } else
	printf(FMT("%8llu ","%llu "), number);
}

/*static*/ int print_match(const struct ipt_entry_match *m,
			   const struct ipt_ip *ip,
			   int numeric)
{
    struct iptables_match *match = find_match(m->u.user.name, TRY_LOAD);

    if (match) {
	if (match->print)
	    match->print(ip, m, numeric);
	else
	    printf("%s ", match->name);
    } else {
	if (m->u.user.name[0])
	    printf("UNKNOWN match `%s' ", m->u.user.name);
    }
    /* Don't stop iterating. */
    return 0;
}

/* e is called `fw' here for hysterical raisins */
/*static*/ void print_firewall(const struct ipt_entry *fw,
			       const char *targname,
			       unsigned int num,
			       unsigned int format,
			       const iptc_handle_t handle)
{
    struct iptables_target *target = NULL;
    const struct ipt_entry_target *t;
    u_int8_t flags;
    char buf[BUFSIZ];

    /* User creates a chain called "REJECT": this overrides the
       `REJECT' target module.  Keep feeding them rope until the
       revolution... Bwahahahahah */
    if (!iptc_is_chain(targname, handle)) {
	  
	target = find_target(targname, TRY_LOAD);
    }else
	target = find_target(IPT_STANDARD_TARGET, LOAD_MUST_SUCCEED);

    t = ipt_get_target((struct ipt_entry *)fw);
    flags = fw->ip.flags;

    if (format & FMT_LINENUMBERS)
	printf(FMT("%-4u ", "%u "), num+1);

    if (!(format & FMT_NOCOUNTS)) {
	print_num(fw->counters.pcnt, format);
	print_num(fw->counters.bcnt, format);
    }

    if (!(format & FMT_NOTARGET))
	printf(FMT("%-9s ", "%s "), targname);

    fputc(fw->ip.invflags & IPT_INV_PROTO ? '!' : ' ', stdout);
    {
	char *pname = proto_to_name(fw->ip.proto, format&FMT_NUMERIC);
	if (pname)
	    printf(FMT("%-5s", "%s "), pname);
	else
	    printf(FMT("%-5hu", "%hu "), fw->ip.proto);
    }

    if (format & FMT_OPTIONS) {
	if (format & FMT_NOTABLE)
	    fputs("opt ", stdout);
	fputc(fw->ip.invflags & IPT_INV_FRAG ? '!' : '-', stdout);
	fputc(flags & IPT_F_FRAG ? 'f' : '-', stdout);
	fputc(' ', stdout);
    }

    if (format & FMT_VIA) {
	char iface[IFNAMSIZ+2];

	if (fw->ip.invflags & IPT_INV_VIA_IN) {
	    iface[0] = '!';
	    iface[1] = '\0';
	}
	else iface[0] = '\0';

	if (fw->ip.iniface[0] != '\0') {
	    strcat(iface, fw->ip.iniface);
	    /* If it doesn't compare the nul-term, it's a
	       wildcard. */
	    if (fw->ip.iniface_mask[strlen(fw->ip.iniface)] == 0)
		strcat(iface, "+");
	}
	else if (format & FMT_NUMERIC) strcat(iface, "*");
	else strcat(iface, "any");
	printf(FMT(" %-6s ","in %s "), iface);

	if (fw->ip.invflags & IPT_INV_VIA_OUT) {
	    iface[0] = '!';
	    iface[1] = '\0';
	}
	else iface[0] = '\0';

	if (fw->ip.outiface[0] != '\0') {
	    strcat(iface, fw->ip.outiface);
	    /* If it doesn't compare the nul-term, it's a
	       wildcard. */
	    if (fw->ip.outiface_mask[strlen(fw->ip.outiface)] == 0)
		strcat(iface, "+");
	}
	else if (format & FMT_NUMERIC) strcat(iface, "*");
	else strcat(iface, "any");
	printf(FMT("%-6s ","out %s "), iface);
    }

    fputc(fw->ip.invflags & IPT_INV_SRCIP ? '!' : ' ', stdout);
    if (fw->ip.smsk.s_addr == 0L && !(format & FMT_NUMERIC))
	printf(FMT("%-19s ","%s "), "anywhere");
    else {
	if (format & FMT_NUMERIC)
	    sprintf(buf, "%s", addr_to_dotted(&(fw->ip.src)));
	else
	    sprintf(buf, "%s", addr_to_anyname(&(fw->ip.src)));
	strcat(buf, mask_to_dotted(&(fw->ip.smsk)));
	printf(FMT("%-19s ","%s "), buf);
    }

    fputc(fw->ip.invflags & IPT_INV_DSTIP ? '!' : ' ', stdout);
    if (fw->ip.dmsk.s_addr == 0L && !(format & FMT_NUMERIC))
	printf(FMT("%-19s","->%s"), "anywhere");
    else {
	if (format & FMT_NUMERIC)
	    sprintf(buf, "%s", addr_to_dotted(&(fw->ip.dst)));
	else
	    sprintf(buf, "%s", addr_to_anyname(&(fw->ip.dst)));
	strcat(buf, mask_to_dotted(&(fw->ip.dmsk)));
	printf(FMT("%-19s","->%s"), buf);
    }

    if (format & FMT_NOTABLE)
	fputs("  ", stdout);

    IPT_MATCH_ITERATE(fw, print_match, &fw->ip, format & FMT_NUMERIC);

    if (target) {
	if (target->print)
	    /* Print the target information. */
	    target->print(&fw->ip, t, format & FMT_NUMERIC);
    } else if (t->u.target_size != sizeof(*t))
	printf("[%u bytes of unknown target data] ",
	       t->u.target_size - sizeof(*t));

    if (!(format & FMT_NONEWLINE))
	fputc('\n', stdout);
}

/*static*/ void print_firewall_line(const struct ipt_entry *fw,
				    const iptc_handle_t h)
{
    struct ipt_entry_target *t;

    t = ipt_get_target((struct ipt_entry *)fw);
    print_firewall(fw, t->u.user.name, 0, FMT_PRINT_RULE, h);
}

#endif

/*static*/ unsigned char *make_delete_mask(struct ipt_entry *fw)
{
    /* Establish mask for comparison */
    unsigned int size;
    struct iptables_match *m;
    unsigned char *mask, *mptr;

    size = sizeof(struct ipt_entry);
    for (m = iptables_matches; m; m = m->next) {
	if (m->used) {
	    size += IPT_ALIGN(sizeof(struct ipt_entry_match)) + m->size;
	}
    }

    mask = (unsigned char *) fw_calloc(1, size
				       + IPT_ALIGN(sizeof(struct ipt_entry_target))
				       + iptables_targets->size);

    memset(mask, 0xFF, sizeof(struct ipt_entry));
    mptr = mask + sizeof(struct ipt_entry);

    for (m = iptables_matches; m; m = m->next) {
	if (m->used) {
	    memset(mptr, 0xFF,
		   IPT_ALIGN(sizeof(struct ipt_entry_match))
		   + m->userspacesize);
	    mptr += IPT_ALIGN(sizeof(struct ipt_entry_match)) + m->size;
	}
    }

    memset(mptr, 0xFF, 
	   IPT_ALIGN(sizeof(struct ipt_entry_target))
	   + iptables_targets->userspacesize);

    return mask;
}

/*static*/ void set_inverse(unsigned int param, u_int8_t *invflg, int invert)
{
   
    if (invert) {
	if (!inverse_for_params[param])
	    exit_error(PARAMETER_PROBLEM, "cannot have !before %s", param_names[param]);

	*invflg | = inverse_for_params[param];
    }
}


/* ------------------------- libiptc wrappers ---------------------------- */

static int append_entry(const ipt_chainlabel chain, struct ipt_entry *fw,
			unsigned int nsaddrs, const struct in_addr saddrs[],
			unsigned int ndaddrs,const struct in_addr daddrs[],
			int verbose, iptc_handle_t *handle)
{
    unsigned int i, j;
    int ret = 1;

    for (i = 0; i < nsaddrs; i++) {
	fw->ip.src.s_addr = saddrs[i].s_addr;
	for (j = 0; j < ndaddrs; j++) {
	    fw->ip.dst.s_addr = daddrs[j].s_addr;
#ifdef CLASS_DEBUG
	    fprintf(stdout, "append: ");
	    print_firewall_line(fw, *handle);
#endif
	    ret & = iptc_append_entry(chain, fw, handle);
	}
    }

    return ret;
}

static int delete_entry(const ipt_chainlabel chain,
			struct ipt_entry *fw,
			unsigned int nsaddrs,
			const struct in_addr saddrs[],
			unsigned int ndaddrs,
			const struct in_addr daddrs[],
			int verbose,
			iptc_handle_t *handle)
{
    unsigned int i, j;
    int ret = 1;
    unsigned char *mask;

    mask = (unsigned char *) make_delete_mask(fw);
    for (i = 0; i < nsaddrs; i++) {
	fw->ip.src.s_addr = saddrs[i].s_addr;
	for (j = 0; j < ndaddrs; j++) {
	    fw->ip.dst.s_addr = daddrs[j].s_addr;
#ifdef CLASS_DEBUG
	    fprintf(stdout, "delete: ");
	    print_firewall_line(fw, *handle);
#endif
			
	    ret & = iptc_delete_entry(chain, fw, mask, handle);
	}
    }
    return ret;
}

int flush_entries(const ipt_chainlabel chain, int verbose,
		  iptc_handle_t *handle)
{
  
#ifdef CLASS_DEBUG
    fprintf(stdout, "Flushing chain `%s'\n", chain);
#endif

    return iptc_flush_entries(chain, handle);
}

/* ------------------------- ClassifierNetfilter ------------------------- */

ClassifierNetfilter::ClassifierNetfilter(ConfigManager *cnf, int threaded ) 
    : Classifier(cnf,"ClassifierNetfilter",threaded)
{
    log(CREATE1 );

    // construct classifier, i.e. init internal structures

    // allocate a receive buffer
    buf = (unsigned char *) new unsigned char[MYBUFSIZ];
    // allocate libiptc handle
    iptc_h = new iptc_handle_t;

    memset(&stats, 0, sizeof(stats));
    memset(iptc_h, 0, sizeof(iptc_handle_t));

    // initialize the parameters

    table = strdup(cnf->getValue("NetfilterTable", "CLASSIFIER").c_str());
    if (strlen(table) == 0) {
	table = new char[strlen(def_tablename)+1];
	strcpy(table, def_tablename);
    }

    chain = strdup(cnf->getValue("NetfilterChain", "CLASSIFIER").c_str());
    if (strlen(chain) == 0) {
	chain = new char[strlen(def_chainname)+1];
	strcpy(chain, def_chainname);
    }

    if (strlen(cnf->getValue("NetfilterGroup", "CLASSIFIER").c_str()) > 0) {
	nlgroup = atoi(cnf->getValue("NetfilterGroup", "CLASSIFIER").c_str());
    } else {
	nlgroup = def_nlgroup;
    }

    if (strlen(conf->getValue("NetfilterCPRange", "CLASSIFIER").c_str()) > 0) {
	cprange = atoi(conf->getValue("NetfilterCPRange", "CLASSIFIER").c_str());
    } else {
	cprange = def_cprange;
    }

    if (strlen(conf->getValue("NetfilterQThres", "CLASSIFIER").c_str()) > 0) {
	qthres = atoi(conf->getValue("NetfilterQThres", "CLASSIFIER").c_str());
    } else {
	qthres = def_nonrtqt;
    }

#ifdef DEBUG2
    fprintf(stderr, "Class_NF is using the following defaults: \n");
    fprintf(stderr, "  Table:   %s\n", table);
    fprintf(stderr, "  Chain:   %s\n", chain);
    fprintf(stderr, "  NLGroup: %d\n", nlgroup);
    fprintf(stderr, "  CPRange: %d\n", cprange);
    fprintf(stderr, "  QThres:  %d\n", qthres);
#endif

    log(CREATE2);
}


/* ------------------------- ~ClassifierNetfilter ------------------------- */

ClassifierNetfilter::~ClassifierNetfilter()
{
    int ret = 1;

    log(SHUTDOWN );

    // flush meter chain
    ret = flush_entries(chain, 0, iptc_h);
    if (ret) {
	ret = iptc_commit(iptc_h);
    }

    stop();

    // destroy buffer
    saveDelete(buf;
    saveDelete(chain;
}


/* ------------------------- go ------------------------- */

void ClassifierNetfilter::run()
{

    if (!running) {
	// create libiptc handle
	*iptc_h = iptc_init(table);
	if (!*iptc_h) {
	    // try to insmod the module if iptc_init failed
	    iptables_insmod("ip_tables", def_modprobe);
	    *iptc_h = iptc_init(table);

	    throw MyErr("cant initialize libiptc: %s", iptc_strerror(errno));
	}

	// create ipulog handle
	h = ipulog_create_handle(ipulog_group2gmask(nlgroup));
	if (!h) {
	    throw MyErr("cant initialize libipulog: %s : %s", 
			 ""/*ipulog_strerror()*/, strerror(errno));
	}  

	MeterComponent::run();
    }
}


/* ------------------------- stop ------------------------- */

void ClassifierNetfilter::stop()
{
  
    if (running) {

	ipulog_destroy_handle(h);
	// destroy handle
	saveDelete(iptc_h;
	iptc_h = NULL;
	  
	MeterComponent::stop();
    }  
}

/* ------------------------- main ------------------------- */

void ClassifierNetfilter::main()
{
    //    unsigned short pkt_len;
    //    static metaData_t meta;
    
    // this function will be run as a single thread inside the classifier
    log("thread started" );

#ifndef LOOPFULL
    for (; ; ) {
	try {

	    len = ipulog_read(h, buf, MYBUFSIZ, 1);
	    //printf("len %d\n", len);
	    if (len < 0) {
		throw MyErr("ipulog_read: short read: %s : %s", 
			     ""/*ipulog_strerror()*/, strerror(errno));
	    }
		
	    while ((upkt = ipulog_get_packet(h, buf, len)) != NULL) {

		myCore()->getPacketProcessor()->processPacket(atoi(upkt->prefix),
							      (char *) upkt->payload,
							      (metaData_t *) upkt);
		stats.packets++;
		stats.bytes += upkt->data_len;
	    }                  	
	} catch (MyErr &e ) {
	    cerr << e << endl;
	}
    }
#else // LOOPFULL
    {
	char buf[100];
	metaData_t meta;
	int i = 1;

	// FIXME: enter better dummy values into struct
	meta.data_len = 100;
	meta.tv_sec = 0;
	meta.tv_usec = 0;

	// wait until a first task has been added
	while (maxid == 0 ) sleep(1);

	while (1) {
	    myCore()->getPacketProcessor()->processPacket(i,buf,&meta);
	    i++;
	    if (i>maxid ) i = 1;
	}
    }
#endif  // LOOPFULL
}


struct netfilter_rule_entry 
*ClassifierNetfilter::alloc_rule_entry(struct ipt_entry *e, unsigned int nsaddrs,
				       struct in_addr *saddrs, unsigned int ndaddrs, 
				       struct in_addr *daddrs)
{
    struct netfilter_rule_entry *nfre;

    nfre = new struct netfilter_rule_entry;

    nfre->e = (struct ipt_entry *) new char[e->next_offset];

    memcpy(nfre->e, e, e->next_offset);

    // FIXME: support only 1 src and 1 dst IP
    nfre->saddrs = *saddrs;
    nfre->daddrs = *daddrs;
    nfre->nsaddrs = 1;
    nfre->ndaddrs = 1;

    return nfre;
} 

void ClassifierNetfilter::delete_rule_entry(struct netfilter_rule_entry *nfre)
{
    // FIXME: support only 1 src and 1 dst IP
    saveDelete(nfre->e;
    saveDelete(nfre;
} 

int ClassifierNetfilter::check_for_invert(char *param_in, char **param_out)
{
    int i = 0;

    // assume leading WS have been stripped
    if (param_in && !strncmp(param_in, "!", 1)) {

	// jump over to param following !ignoring leading WS
	param_in = &param_in[1];
	while (isspace(param_in[i]))
	    i++;
	*param_out = &param_in[i];
	
	return 1;
    } else {
	*param_out = param_in;
	return 0;
    }
}

/* ------------------------- addRule ------------------------- */

// wrapper for handling bidir matches
// FIXME integrate both add methods
void ClassifierNetfilter::addRule( RuleInfo &rinfo, int test )
{
    int  rule_id, nAttributes, i;
    char **attributes, **params;
    int bidir = 0;

    // extract filter rule information from RuleInfo object
    rule_id = rinfo.getId();
    MLOG( "ClassifierNetfilter : adding Rule #%d", rule_id );
    attributes = rinfo.getFilter()->getNames( &nAttributes);
    params = rinfo.getFilter()->getValues( &nAttributes );
  
    if (rule_id<0 || nAttributes<0 || attributes == NULL || params == NULL )
	throw MyErr( "invalid filter rule description" );

    for (i = 0; i<nAttributes; i++) {
	if (!strcmp(attributes[i], param_names[defp_bidir]) ) {
	    // bidir flow match
	    bidir = 1;
	}
    }

#ifdef CLASS_DEBUG   
    fprintf(stderr, "Adding forward rule\n");
#endif

    // add forward
    this->_addRule(rinfo, test);
  
    // add backward if bidir match
    if (bidir) {

#ifdef CLASS_DEBUG
	fprintf(stderr, "Bidirectional match: Adding backward rule\n");
#endif

	for (i = 0; i<nAttributes; i++) {
	    // exchange src with dst ip, exchange iiface with oiface
	    if (!strcmp(attributes[i], param_names[defp_srcipmask]) ) {
		free(attributes[i]);
		attributes[i] = strdup(param_names[defp_dstipmask]);
	    }
	    else if (!strcmp(attributes[i], param_names[defp_dstipmask]) ) {
		free(attributes[i]);
		attributes[i] = strdup(param_names[defp_srcipmask]);
	    }
	    else if (!strcmp(attributes[i], param_names[defp_iniface]) ) {
		free(attributes[i]);
		attributes[i] = strdup(param_names[defp_outiface]);
	    }
	    else if (!strcmp(attributes[i], param_names[defp_outiface]) ) {
		free(attributes[i]);
		attributes[i] = strdup(param_names[defp_iniface]);
	    }
	    // exchange src with dst port
	    // FIXME this is clumsy using parameter names directly
	    else if ((!strcmp(attributes[i], "srcport")) || (!strcmp(attributes[i], "sport")) ) {
		free(attributes[i]);
		attributes[i] = strdup("dport");
	    }
	    else if ((!strcmp(attributes[i], "dstport")) || (!strcmp(attributes[i], "dport")) ) {
		free(attributes[i]);
		attributes[i] = strdup("sport");
	    }
	}
	
	this->_addRule(rinfo, test);
    }
}

void ClassifierNetfilter::_addRule( RuleInfo &rinfo, int test )
{
    int  rule_id, nAttributes, i, argc = 0;
    char **attributes, **params, *param;

    struct ipt_entry fw, *e = NULL;
    int invert = 0;
    unsigned int nsaddrs = 0, ndaddrs = 0;
    struct in_addr *saddrs = NULL, *daddrs = NULL;
    int c, valid;
    const char *shostnetworkmask = NULL, *dhostnetworkmask = NULL;
    unsigned int options = 0;
    int ret = 1;
    struct iptables_match *m = NULL;
    struct iptables_target *target = NULL;
    const char *jumpto = "";
    char *protocol = NULL;
    char **argv;
    struct netfilter_rule_entry *nfre;

#ifdef LOOPFULL
    maxid++;
    return;
#endif
   
    // extract filter rule information from RuleInfo object
    rule_id = rinfo.getId();
    //    MLOG( "ClassifierNetfilter : adding Rule #%d", rule_id );
    attributes = rinfo.getFilter()->getNames( &nAttributes);
    params = rinfo.getFilter()->getValues( &nAttributes );

    if (rule_id<0 || nAttributes<0 || attributes == NULL || params == NULL )
	throw MyErr( "invalid filter rule description" );
	
#ifdef CLASS_DEBUG
    cout << *(rinfo.getFilter()) << "\n"; // dump filter spec
#endif

    // emulate getopt
    // allocate some space for optarg, argv
    //if (optarg) //seems to be dangerous assumption
    //  free(optarg);
    optarg = new char[256];
    argv = new char*[nAttributes*2];
    optind = 1;

    for (i = 0; i<nAttributes; i++) {
	invert = check_for_invert(params[i], &param);
	if (invert) {
	    argv[argc] = new char[strlen(inv_symbol)+1];
	    strcpy(argv[argc++], inv_symbol);
	}
	if (param != NULL) {
	    argv[argc] = new char[strlen(param)+1];
	    strcpy(argv[argc++], param);
	}
    }

    /* Suppress error messages: we may add new options if we
       demand-load a protocol. */
    opterr = 0;

    // initialize data structures
    memset(&fw, 0, sizeof(fw));
    shostnetworkmask = "0.0.0.0/0"; 	
    dhostnetworkmask = "0.0.0.0/0";

    // reset used flag for matches, used = 1 means used by current rule
    for (m = iptables_matches; m; m = m->next) {
	m->used = 0;
    }

    // target is _always_ ULOG ->load it
    jumpto = parse_target(def_targetname);
    if ((target = find_target(jumpto, DONT_LOAD))) {
	// initialize data and flags
	target->tflags = 0;
	memset(target->t->data, 0, target->size);
	target->init(target->t, &fw.nfcache);
	target->used = 1;
    } else {
	if ((target = find_target(jumpto, LOAD_MUST_SUCCEED))) {
	    size_t size;
#ifdef CLASS_DEBUG
	    fprintf(stderr, "%s loaded\n", jumpto);
#endif	  
	    size = IPT_ALIGN(sizeof(struct ipt_entry_target)) + target->size;
	    target->t = (struct ipt_entry_target *) fw_calloc(1, size);
	    target->t->u.target_size = size;
	    strcpy(target->t->u.user.name, jumpto);
	    // call init function
	    target->init(target->t, &fw.nfcache);
	    target->used = 1;
	}
	else
	    throw MyErr("cant load %s target", def_targetname);
    }
	  
#ifdef CLASS_DEBUG
    fprintf(stderr, "rule target initialized\n");
#endif

    // configure target, set nlgroup
    c = translate_option((char *)param_names[defp_nlgroup], target->option_offset);
    if (c<0)
	throw MyErr("Unknown arg `%s'", param_names[defp_nlgroup]);
    sprintf(optarg, "%d", nlgroup);
#ifdef CLASS_DEBUG
    fprintf(stderr, "nlgroup option resolved to %d, arg is %s\n", c, optarg);
#endif
    if (!(target->parse(c - target->option_offset, argv, invert,
			&target->tflags, &fw, &target->t))) {
	throw MyErr("Unknown arg `%s'", param_names[defp_nlgroup]);
    }

    // set rule id
    c = translate_option((char *)param_names[defp_prefix], target->option_offset);
    if (c<0)
	throw MyErr("Unknown arg `%s'", param_names[defp_prefix]);
    sprintf(optarg, "%d", rule_id);
#ifdef CLASS_DEBUG
    fprintf(stderr, "prefix option resolved to %d, arg is %s\n", c, optarg);
#endif
    if (!(target->parse(c - target->option_offset, argv, invert,
			&target->tflags, &fw, &target->t))) {
	throw MyErr("Unknown arg `%s'", param_names[defp_prefix]);
    }

    // default cprange
    c = translate_option((char *)param_names[defp_cprange], target->option_offset);
    if (c<0)
	throw MyErr("Unknown arg `%s'", param_names[defp_cprange]);
    sprintf(optarg, "%d", cprange);
#ifdef CLASS_DEBUG
    fprintf(stderr, "cprange option resolved to %d, arg is %s\n", c, optarg);
#endif
    if (!(target->parse(c - target->option_offset, argv, invert,
			&target->tflags, &fw, &target->t))) {
	throw MyErr("Unknown arg `%s'", param_names[defp_cprange]);
    }

    // default qthreshold
    c = translate_option((char *)param_names[defp_qthreshold], target->option_offset);
    if (c<0)
	throw MyErr("Unknown arg `%s'", param_names[defp_qthreshold]);
    sprintf(optarg, "%d", qthres);
#ifdef CLASS_DEBUG
    fprintf(stderr, "qthreshold option resolved to %d, arg is %s\n", c, optarg);
#endif
    if (!(target->parse(c - target->option_offset, argv, invert,
			&target->tflags, &fw, &target->t))) {
	throw MyErr("Unknown arg `%s'", param_names[defp_qthreshold]);
    }

    // reset target (ulog) flags to allow cprange and qthreshold config by
    // rule
    target->tflags = 0;

    // check the given attributes + parse and check the parameter values
    for (i = 0; i<nAttributes; i++ ) {

	invert = check_for_invert(params[i], &param);
	if (invert)
	    optind++;

	if (!strcmp( attributes[i], "nofilter" ) ) {
	    // ignore 'nofilter'
	}
	else if (!strcmp( attributes[i], param_names[defp_bidir] ) ) {
	    // ignore at this point
	}
	else if (!strcmp( attributes[i], param_names[defp_srcipmask] ) ) {
	    set_inverse(defp_srcipmask, &fw.ip.invflags, invert);
	    shostnetworkmask = param;
	    fw.nfcache | = NFC_IP_DST;
	}
	else if (!strcmp( attributes[i], param_names[defp_dstipmask] ) ) {
	    set_inverse(defp_dstipmask, &fw.ip.invflags, invert);
	    dhostnetworkmask = param;
	    fw.nfcache | = NFC_IP_DST;
	}
	else if (!strcmp( attributes[i], param_names[defp_iniface] ) ) {
	    set_inverse(defp_iniface, &fw.ip.invflags, invert);
	    parse_interface(param, fw.ip.iniface, fw.ip.iniface_mask);
	    fw.nfcache | = NFC_IP_IF_IN;
	}
	else if (!strcmp( attributes[i], param_names[defp_outiface] ) ) {
	    set_inverse(defp_outiface, &fw.ip.invflags, invert);
	    parse_interface(param, fw.ip.outiface, fw.ip.outiface_mask);
	    fw.nfcache | = NFC_IP_IF_OUT;
	}
	else if (!strcmp( attributes[i], param_names[defp_match] ) ) {
	    size_t size;

	    if (invert)
		exit_error(PARAMETER_PROBLEM, "unexpected !flag before %s", 
			   param_names[defp_match]);

	    if ((m = find_match(param, DONT_LOAD))) {
		// initialize data and flags
		m->mflags = 0;
		memset(m->m->data, 0, m->size);
		m->init(m->m, &fw.nfcache);
		m->used = 1;
	    } else {
		if ((m = find_match(param, LOAD_MUST_SUCCEED))) {
		    size = IPT_ALIGN(sizeof(struct ipt_entry_match)) + m->size;
		    m->m = (struct ipt_entry_match *) fw_calloc(1, size);
		    m->m->u.match_size = size;
		    strcpy(m->m->u.user.name, m->name);
		    // call init function
		    m->init(m->m, &fw.nfcache);
		    m->used = 1;
		}
	    }
	}
	else if (!strcmp( attributes[i], param_names[defp_prot] ) ) {
	    set_inverse(defp_prot, &fw.ip.invflags, invert);

	    /* Canonicalize into lower case */
	    for (protocol = param; *protocol; protocol++)
		*protocol = tolower(*protocol);
		
	    protocol = param;
	    fw.ip.proto = parse_protocol(protocol);
	    if ((fw.ip.proto == 0) && (fw.ip.invflags & IPT_INV_PROTO))
		exit_error(PARAMETER_PROBLEM, "rule would never match protocol");
	    fw.nfcache | = NFC_IP_PROTO;

	}
	else {
	    // must be a module specific option or ULOG option
	     
	    // set matches according to protocol
	    if (m == NULL && protocol) {
		if ((m = find_proto(protocol, DONT_LOAD, options & 0x01))) {
		    // initialize data and flags
		    m->mflags = 0;
		    memset(m->m->data, 0, m->size);
		    m->init(m->m, &fw.nfcache);
		    m->used = 1;
		} else {
		    if ((m = find_proto(protocol, TRY_LOAD, options & 0x01))) {
			size_t size;
#ifdef CLASS_DEBUG
			fprintf(stderr, "loading %s module\n", protocol);
#endif			
			size = IPT_ALIGN(sizeof(struct ipt_entry_match)) + m->size;
			m->m = (struct ipt_entry_match *) fw_calloc(1, size);
			m->m->u.match_size = size;
			strcpy(m->m->u.user.name, m->name);
			m->init(m->m, &fw.nfcache);
			m->used = 1;
		    } else {
			exit_error(PARAMETER_PROBLEM, "Error loading %s module", 
				   protocol);
		    }
		}	  
	    }

	    valid = 0;
#ifdef CLASS_DEBUG 
	    fprintf(stderr, "Parsing %s option\n", attributes[i]);
#endif
	    for (m = iptables_matches; m; m = m->next) {
		if (m->used) {
		    c = translate_option(attributes[i], m->option_offset);
		    if (c > 0) {
			strcpy(optarg, param);
    
			// if protocol is inverse options for this protocol makes no sense
			if (fw.ip.invflags & IPT_INV_PROTO) {
			    exit_error(PARAMETER_PROBLEM, 
				       "Option specified for negative protocol match '%s'",
				       protocol);
			}
				
			if (m->parse(c - m->option_offset,
				     argv, invert, &m->mflags, &fw, &fw.nfcache, &m->m)) {
				  
			    valid = 1;
			    break;
			}
		    }
		}
	    }
		  
	    // following target parameters: qthreshold, cprange
	    if (!valid) {
		c = translate_option(attributes[i], target->option_offset);
		if (c > 0) {
		    strcpy(optarg, param);

		    if (target->parse(c - target->option_offset, argv, invert,
				      &target->tflags, &fw, &target->t)) {
			valid = 1;
		    }
		}
	    }
		  
	    if (!valid)
		exit_error(PARAMETER_PROBLEM, "Unknown arg `%s'", attributes[i]);
	}
	  
	optind++;
    }

    // final check for all modules used
    for (m = iptables_matches; m; m = m->next) {
	if (m->used) {
	    m->final_check(m->mflags);
	}
    }
    if (target)
	target->final_check(target->tflags);

    // parse source and destination address+mask
    if (shostnetworkmask)
	parse_hostnetworkmask(shostnetworkmask, &saddrs,
			      &(fw.ip.smsk), &nsaddrs);

    if (dhostnetworkmask)
	parse_hostnetworkmask(dhostnetworkmask, &daddrs,
			      &(fw.ip.dmsk), &ndaddrs);
#ifdef CLASS_DEBUG
    fprintf(stderr, "all parameters parsed\n");
#endif
    // FIXME: dont support this right now
    /* if ((nsaddrs > 1 || ndaddrs > 1) &&
       (fw.ip.invflags & (IPT_INV_SRCIP | IPT_INV_DSTIP)))
       exit_error(PARAMETER_PROBLEM, "!not allowed with multiple"
       " source or destination IP addresses"); */

    // check for valid interface specs according to target chain
    if (strcmp(chain, "PREROUTING") == 0 || strcmp(chain, "INPUT") == 0 ||
	strcmp(chain, "PROMISCUOUS") ) {
	if (fw.nfcache & NFC_IP_IF_OUT)
	    exit_error(PARAMETER_PROBLEM, "Can't use %s with %s\n",
		       param_names[defp_outiface], chain);
    }
    if (strcmp(chain, "POSTROUTING") == 0 || strcmp(chain, "OUTPUT") == 0 ||
	strcmp(chain, "PROMISCUOUS") ) {
	if (fw.nfcache & NFC_IP_IF_IN)
	    exit_error(PARAMETER_PROBLEM, "Can't use %s with %s\n",
		       param_names[defp_iniface], chain);
    }
	
    // do generate because we know that we have a target
    e = generate_entry(&fw, iptables_matches, target->t);

#ifdef CLASS_DEBUG
    fprintf(stderr, "new entry generated\n");
#endif
    nfre = alloc_rule_entry(e, nsaddrs, saddrs, ndaddrs, daddrs);
#ifdef CLASS_DEBUG
    fprintf(stderr, "new rule entry allocated\n");
#endif

    // install NetFilter rule
    ret = append_entry(chain, e, nsaddrs, saddrs, ndaddrs, daddrs,
		       options, iptc_h);

    if (!ret) {
	delete_rule_entry(nfre);
	exit_error(PARAMETER_PROBLEM, "%s\n", iptc_strerror(errno));
    }

    ret = iptc_commit(iptc_h);
    // respawn handle, thanx rusty
    *iptc_h = iptc_init(table);

    if (!ret) {
	delete_rule_entry(nfre);
	exit_error(PARAMETER_PROBLEM, "%s\n", iptc_strerror(errno));
    }

    // if success ->add filter rule to internal rule list
    rule_list.insert(make_pair(rule_id, nfre));
    stats.rules++;

    // if in test mode immediatly delete the rule
    // FIXME: maybe escape earlier because of performance
    if (test) {
	this->delRule(rule_id);
    }

	
    saveDelete(optarg;
    for (i = 0; i<argc; i++) {
	saveDelete(argv[i];
    }
    saveDelete(argv;
}


/* ------------------------- delRule ------------------------- */

void ClassifierNetfilter::delRule( int ruleID )
{
    int ret = 1;
    unsigned int options = 0;
    struct netfilter_rule_entry *nfre;
    rule_map_itor_t itor;

    MLOG( "ClassifierNetfilter : removing Rule #%d", ruleID );

    // get rule ipt_entry
#ifdef CLASS_DEBUG
    fprintf(stderr, "delete rule %d\n", ruleID);
#endif

#ifdef LOOPFULL
    maxid--;
    return;
#endif

    // delete all rules with that ruleID
    // there could be 1 rule in case of unidir match or
    // 2 rules in case of bidir match
    while ((itor = rule_list.find(ruleID)) != rule_list.end()) {

	nfre = itor->second;

#ifdef CLASS_DEBUG
	fprintf(stderr, "rule %d found\n", ruleID);
#endif	

	// delete NetFilter rule
	ret = delete_entry(chain, nfre->e, nfre->nsaddrs, &nfre->saddrs,
			   nfre->ndaddrs, &nfre->daddrs, options, iptc_h);
	  
	if (!ret)
	    exit_error(PARAMETER_PROBLEM, "%s\n", iptc_strerror(errno));

	ret = iptc_commit(iptc_h);
	// respawn handle, thanx rusty
	*iptc_h = iptc_init(table);

	if (!ret)
	    exit_error(PARAMETER_PROBLEM, "%s\n", iptc_strerror(errno));
	
#ifdef CLASS_DEBUG
	fprintf(stderr, "rule deleted from netfilter\n");
#endif

	// delete entry from internal rule_list
	delete_rule_entry(nfre);
	rule_list.erase(itor);
	stats.rules--;

#ifdef CLASS_DEBUG
	fprintf(stderr, "rule deleted from internal list\n");
#endif

    }
}


/* ------------------------- getStats ------------------------- */

ClassifierStats *ClassifierNetfilter::getStats()
{
    // adjust values

    return &stats;
}


/* ------------------------- dump ------------------------- */

void ClassifierNetfilter::dump( ostream &os )
{
    os << "Classifier dump : packets " << stats.packets << endl;
    os << "                  bytes " << stats.bytes << endl;
    os << "                  rules " << stats.rules << endl;
    // write classifier information to os stream

}

/* ------------------------- operator<< ------------------------- */

ostream& operator<< ( ostream &os, ClassifierNetfilter &cl )
{
    cl.dump(os);
    return os;
}

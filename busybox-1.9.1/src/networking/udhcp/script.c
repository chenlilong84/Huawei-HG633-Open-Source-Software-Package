/* vi: set sw=4 ts=4: */
/* script.c
 *
 * Functions to call the DHCP client notification scripts
 *
 * Russ Dill <Russ.Dill@asu.edu> July 2001
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "common.h"
#include "dhcpc.h"
#include "options.h"


/* get a rough idea of how long an option will be (rounding up...) */
static const uint8_t max_option_length[] = {
	[OPTION_IP] =		sizeof("255.255.255.255 "),
	[OPTION_IP_PAIR] =	sizeof("255.255.255.255 ") * 2,
	[OPTION_STRING] =	1,
	/* Modified by y00183935@2014.6.30 for CVE-2011-2716 */
    [OPTION_STRING_HOST] =	1,
    /* Modified by y00183935@2014.6.30 for CVE-2011-2716 */
#if ENABLE_FEATURE_RFC3397
	[OPTION_STR1035] =	1,
#endif
	[OPTION_BOOLEAN] =	sizeof("yes "),
	[OPTION_U8] =		sizeof("255 "),
	[OPTION_U16] =		sizeof("65535 "),
	[OPTION_S16] =		sizeof("-32768 "),
	[OPTION_U32] =		sizeof("4294967295 "),
	[OPTION_S32] =		sizeof("-2147483684 "),
};


static inline int upper_length(int length, int opt_index)
{
	return max_option_length[opt_index] *
		(length / dhcp_option_lengths[opt_index]);
}


static int sprintip(char *dest, const char *pre, const uint8_t *ip)
{
	return sprintf(dest, "%s%d.%d.%d.%d", pre, ip[0], ip[1], ip[2], ip[3]);
}


/* really simple implementation, just count the bits */
static int mton(uint32_t mask)
{
	int i = 0;
	mask = ntohl(mask); /* 111110000-like bit pattern */
	while (mask) {
		i++;
		mask <<= 1;
	}
	return i;
}

/* Modified by y00183935@2014.6.30 for CVE-2011-2716 */
/* Check if a given label represents a valid DNS label
* Return pointer to the first character after the label upon success,
* NULL otherwise.
* See RFC1035, 2.3.1
*/
/* We don't need to be particularly anal. For example, allowing _, hyphen
* at the end, or leading and trailing dots would be ok, since it
* can't be used for attacks. (Leading hyphen can be, if someone uses
* cmd "$hostname"
* in the script: then hostname may be treated as an option)
*/
static const char *valid_domain_label(const char *label)
{
    unsigned char ch;
    unsigned pos = 0;

    for (;;) {
        ch = *label;
        if ((ch|0x20) < 'a' || (ch|0x20) > 'z') {
            if (pos == 0) {
                /* label must begin with letter */
                return NULL;
            }
            if (ch < '0' || ch > '9') {
                if (ch == '\0' || ch == '.')
                    return label;
                /* DNS allows only '-', but we are more permissive */
                if (ch != '-' && ch != '_')
                    return NULL;
            }
        }
        label++;
        pos++;
        //Do we want this?
        //if (pos > 63) /* NS_MAXLABEL; labels must be 63 chars or less */
        //      return NULL;
    }
}

/* Check if a given name represents a valid DNS name */
/* See RFC1035, 2.3.1 */
static int good_hostname(const char *name)
{
    //const char *start = name;

    for (;;) {
        name = valid_domain_label(name);
        if (!name)
            return 0;
        if (!name[0])
            return 1;
        //Do we want this?
        //return ((name - start) < 1025); /* NS_MAXDNAME */
        name++;
    }
}
/* Modified by y00183935@2014.6.30 for CVE-2011-2716 */

/* Allocate and fill with the text of option 'option'. */
static char *alloc_fill_opts(uint8_t *option, const struct dhcp_option *type_p, const char *opt_name)
{
	int len, type, optlen;
	uint16_t val_u16;
	int16_t val_s16;
	uint32_t val_u32;
	int32_t val_s32;
	char *dest, *ret;

	len = option[OPT_LEN - 2];
	type = type_p->flags & TYPE_MASK;
	optlen = dhcp_option_lengths[type];

	dest = ret = xmalloc(upper_length(len, type) + strlen(opt_name) + 2);
	dest += sprintf(ret, "%s=", opt_name);

	for (;;) {
		switch (type) {
		case OPTION_IP_PAIR:
			dest += sprintip(dest, "", option);
			*dest++ = '/';
			option += 4;
			optlen = 4;
		case OPTION_IP:	/* Works regardless of host byte order. */
			dest += sprintip(dest, "", option);
			break;
		case OPTION_BOOLEAN:
			dest += sprintf(dest, *option ? "yes" : "no");
			break;
		case OPTION_U8:
			dest += sprintf(dest, "%u", *option);
			break;
		case OPTION_U16:
			memcpy(&val_u16, option, 2);
			dest += sprintf(dest, "%u", ntohs(val_u16));
			break;
		case OPTION_S16:
			memcpy(&val_s16, option, 2);
			dest += sprintf(dest, "%d", ntohs(val_s16));
			break;
		case OPTION_U32:
			memcpy(&val_u32, option, 4);
			dest += sprintf(dest, "%lu", (unsigned long) ntohl(val_u32));
			break;
		case OPTION_S32:
			memcpy(&val_s32, option, 4);
			dest += sprintf(dest, "%ld", (long) ntohl(val_s32));
			break;
		case OPTION_STRING:
		/* Modified by y00183935@2014.6.30 for CVE-2011-2716 */
        case OPTION_STRING_HOST:
		/* Modified by y00183935@2014.6.30 for CVE-2011-2716 */
			memcpy(dest, option, len);
			dest[len] = '\0';
			/* Modified by y00183935@2014.6.30 for CVE-2011-2716 */
            if (type == OPTION_STRING_HOST && !good_hostname(dest))
                safe_strncpy(dest, "bad", len);
			/* Modified by y00183935@2014.6.30 for CVE-2011-2716 */
			return ret;	 /* Short circuit this case */
#if ENABLE_FEATURE_RFC3397
		case OPTION_STR1035:
			/* unpack option into dest; use ret for prefix (i.e., "optname=") */
			dest = dname_dec(option, len, ret);
			free(ret);
			return dest;
#endif
		}
		option += optlen;
		len -= optlen;
		if (len <= 0) break;
		dest += sprintf(dest, " ");
	}
	return ret;
}


/* put all the parameters into an environment */
static char **fill_envp(struct dhcpMessage *packet)
{
	int num_options = 0;
	int i, j;
	char **envp;
	char *var;
	const char *opt_name;
	uint8_t *temp;
	char over = 0;

	if (packet) {
		for (i = 0; dhcp_options[i].code; i++) {
			if (get_option(packet, dhcp_options[i].code)) {
				num_options++;
				if (dhcp_options[i].code == DHCP_SUBNET)
					num_options++; /* for mton */
			}
		}
		if (packet->siaddr)
			num_options++;
		temp = get_option(packet, DHCP_OPTION_OVER);
		if (temp)
			over = *temp;
		if (!(over & FILE_FIELD) && packet->file[0])
			num_options++;
		if (!(over & SNAME_FIELD) && packet->sname[0])
			num_options++;
	}

	envp = xzalloc(sizeof(char *) * (num_options + 5));
	j = 0;
	envp[j++] = xasprintf("interface=%s", client_config.interface);
	var = getenv("PATH");
	if (var)
		envp[j++] = xasprintf("PATH=%s", var);
	var = getenv("HOME");
	if (var)
		envp[j++] = xasprintf("HOME=%s", var);

	if (packet == NULL)
		return envp;

	envp[j] = xmalloc(sizeof("ip=255.255.255.255"));
	sprintip(envp[j++], "ip=", (uint8_t *) &packet->yiaddr);

	opt_name = dhcp_option_strings;
	i = 0;
	while (*opt_name) {
		temp = get_option(packet, dhcp_options[i].code);
		if (!temp)
			goto next;
		envp[j++] = alloc_fill_opts(temp, &dhcp_options[i], opt_name);

		/* Fill in a subnet bits option for things like /24 */
		if (dhcp_options[i].code == DHCP_SUBNET) {
			uint32_t subnet;
			memcpy(&subnet, temp, 4);
			envp[j++] = xasprintf("mask=%d", mton(subnet));
		}
 next:
		opt_name += strlen(opt_name) + 1;
		i++;
	}
	if (packet->siaddr) {
		envp[j] = xmalloc(sizeof("siaddr=255.255.255.255"));
		sprintip(envp[j++], "siaddr=", (uint8_t *) &packet->siaddr);
	}
	if (!(over & FILE_FIELD) && packet->file[0]) {
		/* watch out for invalid packets */
		packet->file[sizeof(packet->file) - 1] = '\0';
		envp[j++] = xasprintf("boot_file=%s", packet->file);
	}
	if (!(over & SNAME_FIELD) && packet->sname[0]) {
		/* watch out for invalid packets */
		packet->sname[sizeof(packet->sname) - 1] = '\0';
		envp[j++] = xasprintf("sname=%s", packet->sname);
	}
	return envp;
}


/* Call a script with a par file and env vars */
void udhcp_run_script(struct dhcpMessage *packet, const char *name)
{
	int pid;
	char **envp, **curr;

	if (client_config.script == NULL)
		return;

	DEBUG("vfork'ing and execle'ing %s", client_config.script);

	envp = fill_envp(packet);

	/* call script */
// can we use wait4pid(spawn(...)) here?
	pid = vfork();
	if (pid < 0) return;
	if (pid == 0) {
		/* close fd's? */
		/* exec script */
		execle(client_config.script, client_config.script,
		       name, NULL, envp);
		bb_perror_msg_and_die("script %s failed", client_config.script);
	}
	waitpid(pid, NULL, 0);
	for (curr = envp; *curr; curr++)
		free(*curr);
	free(envp);
}

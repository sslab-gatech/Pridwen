int *__h_errno_location(void);
#define h_errno (*__h_errno_location())

struct hostent {
	char *h_name;
	char **h_aliases;
	int h_addrtype;
	int h_length;
	char **h_addr_list;
};

/* A simple news-searcher, written in C.
** For more details about O'Reilly's excellent Meerkat news service, see:
** http://www.oreillynet.com/pub/a/rss/2000/11/14/meerkat_xmlrpc.html */

#include <stdlib.h>
#include <stdio.h>

#include <xmlrpc.h>
#include <xmlrpc_client.h>

#define NAME        "XML-RPC C Meerkat Query Demo"
#define VERSION     "0.1"
#define MEERKAT_URL "http://www.oreillynet.com/meerkat/xml-rpc/server.php"

/* We're a command-line utility, so we abort if an error occurs. */
static void die_if_fault_occurred (xmlrpc_env *env)
{
    if (env->fault_occurred) {
        fprintf(stderr, "XML-RPC Fault #%d: %s\n",
                env->fault_code, env->fault_string);
        exit(1);
    }
}

/* Print a usage message. */
static void usage (void)
{
    fprintf(stderr, "Usage: query-meerkat mysql-regex [hours]\n");
    fprintf(stderr, "Try 'query-meerkat /[Ll]inux/ 24'.\n");
    fprintf(stderr, "Data from <http://www.oreillynet.com/meerkat/>.\n");
    exit(1);
}

/* Hey! We fit in one function. */
int main (int argc, char** argv)
{
    int hours;
    char time_period[16];
    xmlrpc_env env;
    xmlrpc_value *stories, *story;
    size_t size, i;
    char *title, *link, *description;
    int first;

    /* Get our command line arguments. */
    if (argc < 2 || argc > 3)
	usage();
    if (strcmp(argv[1], "--help") == 0)
	usage();
    if (argc == 3)
	hours = atoi(argv[2]);
    else
	hours = 24;
    if (hours == 0)
	usage();
    if (hours > 49) {
        fprintf(stderr, "It's not nice to ask for > 49 hours at once.\n");
        exit(1);	
    }
    snprintf(time_period, sizeof(time_period), "%dHOUR", hours);

    /* Set up our client. */
    xmlrpc_client_init(XMLRPC_CLIENT_NO_FLAGS, NAME, VERSION);
    xmlrpc_env_init(&env);

    /* Ask Meerkat to look for matching stories. */
    stories = xmlrpc_client_call(&env, MEERKAT_URL,
				 "meerkat.getItems", "({s:s,s:i,s:s})",
				 "search", argv[1],
				 "descriptions", (xmlrpc_int32) 76,
				 "time_period", time_period);
    die_if_fault_occurred(&env);
    
    /* Loop over the stories. */
    size = xmlrpc_array_size(&env, stories);
    die_if_fault_occurred(&env);
    first = 1;
    for (i = 0; i < size; i++) {

	/* Extract the useful information from our story. */
	story = xmlrpc_array_get_item(&env, stories, i);
	die_if_fault_occurred(&env);
	xmlrpc_parse_value(&env, story, "{s:s,s:s,s:s,*}",
			   "title", &title,
			   "link", &link,
			   "description", &description);
	die_if_fault_occurred(&env);

	/* Print a separator line if necessary. */
	if (first)
	    first = 0;
	else
	    printf("\n");

	/* Print the story. */
	if (strlen(description) > 0) {
	    printf("%s\n%s\n%s\n", title, description, link);
	} else {
	    printf("%s\n%s\n", title, link);
	}
    }
    
    /* Shut down our client. */
    xmlrpc_DECREF(stories);
    xmlrpc_env_clean(&env);
    xmlrpc_client_cleanup();

    return 0;
}

/* 
  This example program demonstrates the JSON parsing and generating
  capabilities of Xmlrpc-c.

  The program reads JSON text from Standard Input and displays its value as
  XML-RPC XML text.  It then re-generates JSON from the intermediate
  parsed information and displays that.
*/
#include <stdlib.h>
#include <stdio.h>

#include <xmlrpc-c/json.h>
#include <xmlrpc-c/util.h>



static void 
dieIfFaultOccurred(xmlrpc_env * const envP) {
    if (envP->fault_occurred) {
        fprintf(stderr, "ERROR: %s (%d)\n",
                envP->fault_string, envP->fault_code);
        exit(1);
    }
}



void
printAsXml(xmlrpc_value * const valP) {

    xmlrpc_env env;
    xmlrpc_mem_block * outP;

    xmlrpc_env_init(&env);
    
    outP = XMLRPC_MEMBLOCK_NEW(char, &env, 0);

    dieIfFaultOccurred(&env);

    xmlrpc_serialize_value(&env, outP, valP);

    printf("XML-RPC XML:\n");

    printf("%.*s\n",
           XMLRPC_MEMBLOCK_SIZE(char, outP),
           XMLRPC_MEMBLOCK_CONTENTS(char, outP));

    XMLRPC_MEMBLOCK_FREE(char, outP);
    xmlrpc_env_clean(&env);
}



void
printAsJson(xmlrpc_value * const valP) {

    xmlrpc_env env;
    xmlrpc_mem_block * outP;
    xmlrpc_value * val2P;

    xmlrpc_env_init(&env);

    outP = XMLRPC_MEMBLOCK_NEW(char, &env, 0);

    dieIfFaultOccurred(&env);

    xmlrpc_serialize_json(&env, valP, outP);

    dieIfFaultOccurred(&env);

    printf("JSON:\n");

    printf("%.*s\n",
           XMLRPC_MEMBLOCK_SIZE(char, outP),
           XMLRPC_MEMBLOCK_CONTENTS(char, outP));

    XMLRPC_MEMBLOCK_FREE(char, outP);
    xmlrpc_env_clean(&env);
}



int
main(int argc, const char *argv[]) {

    xmlrpc_env env;
    char buf[65536];
    xmlrpc_value * valP;
    size_t bytesReadCt;

    xmlrpc_env_init(&env);

    if (argc-1 > 0) {
        fprintf(stderr, "This program has no arguments.  "
                "JSON input is from Standard Input\n");
        exit(1);
    }
    
    bytesReadCt = fread(buf, 1, sizeof(buf)-1, stdin);
    buf[bytesReadCt] = '\0';
        
    valP = xmlrpc_parse_json(&env, buf);

    dieIfFaultOccurred(&env);

    printAsXml(valP);

    printAsJson(valP);

    xmlrpc_DECREF(valP);
    xmlrpc_env_clean(&env);
    
    return 0;
}



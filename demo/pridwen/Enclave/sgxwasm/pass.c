#include <sgxwasm/pass.h>
#include <stdarg.h>

// Forward declaration.
PASS_LIST(DECLAR_ADD_PASS);

static DEFINE_VECTOR_GROW(pass_manager, struct PassManager);
static DEFINE_VECTOR_INIT(dep_list, struct DepList);
static DEFINE_VECTOR_GROW(dep_list, struct DepList);

#if MEMORY_TRACE
// size_t prev_count = 0;
#endif

// Debugging functions.

void
dump_passes(struct PassManager* pm)
{
  size_t i, j;

  for (i = 0; i < pm->size; i++) {
    struct Pass* pass = &pm->data[i];
#if __DEMO__
    printf("[PRIDWEN] pass #%zu: %s\n", i, pass->name);
#else
    printf("pass #%zu: %s\n", i, pass->name);
#endif
#if 0
    printf("dep list: ");
    struct DepList* dep_list = pass->dep_list;
    for (j = 0; j < dep_list->size; j++) {
      char* dep_name = dep_list->data[j];
      printf("%s ", dep_name);
    }
    printf("\n");
#endif
  }
}

void
dump_active_passes(struct PassManager* pm)
{
  size_t i, j;

  for (i = 0; i < pm->size; i++) {
    struct Pass* pass = &pm->data[i];
    if (pass->active == 0) {
      continue;
    }
#if __DEMO__
    printf("%s ", pass->name);
#else
    printf("pass #%zu: %s\n", i, pass->name);
#endif
#if 0
    printf("dep list: ");
    struct DepList* dep_list = pass->dep_list;
    for (j = 0; j < dep_list->size; j++) {
      char* dep_name = dep_list->data[j];
      printf("%s ", dep_name);
    }
    printf("\n");
#endif
  }
}

void
pass_log(const char* pass_name, char* fmt, ...)
{
#if __SGX__
  char buf[256] = {'\0'};
  va_list ap;

  va_start(ap, fmt);
  vsnprintf(buf, 256, fmt, ap);
  va_end(ap);
  ocall_print_string(buf);
#else
  va_list ap;
  printf("[pass %s] ", pass_name);
  va_start(ap, fmt);
  vfprintf(stdout, fmt, ap);
  va_end(ap);
#endif
}

// End of debugging functions.

// TODO: Add configuration to determine the list of passes to be added.
void
pass_manager_init(struct PassManager* pm)
{
  pm->capacity = 0;
  pm->size = 0;
  pm->data = NULL;

  PASS_LIST(ADD_PASS);
}

struct Pass*
get_new_pass(struct PassManager* pm)
{
  if (!pass_manager_grow(pm))
    return NULL;
  return &pm->data[pm->size - 1];
}

void
passes_init(struct PassManager* pm)
{
  size_t i;
  for (i = 0; i < pm->size; i++) {
    struct Pass* pass = &pm->data[i];
    if (!pass->pass_initialize) {
      continue;
    }
    (pass->pass_initialize)(pm);
  }
}

void
passes_end(struct PassManager* pm)
{
  size_t i;
  for (i = 0; i < pm->size; i++) {
    struct Pass* pass = &pm->data[i];
    if (!pass->pass_end) {
      continue;
    }
    (pass->pass_end)(pm);
  }
}

void
pass_disable(struct PassManager* pm, char* pass_name)
{
  size_t i;
  for (i = 0; i < pm->size; i++) {
    struct Pass* pass = &pm->data[i];
    if (strcmp(pass->name, pass_name) == 0) {
      pass->active = 0;
    }
  }
}

int
pass_is_enabled(struct PassManager* pm, char* pass_name)
{
  size_t i;
  for (i = 0; i < pm->size; i++) {
    struct Pass* pass = &pm->data[i];
    if (pass->active == 0) {
      continue;
    }
    if (strcmp(pass->name, pass_name) == 0) {
      return 1;
    }
  }
  return 0;
}

void
pass_init(struct Pass* pass,
          const char* name,
          void* pass_initialize,
          void* pass_end,
          void* function_start,
          void* function_end,
          void* control_start,
          void* control_end,
          void* instruction_start,
          void* instruction_end,
          void* machine_inst_start,
          void* machine_inst_end,
          void *validation)
{
  pass->name = name;
  pass->active = 1;
  pass->pass_initialize = pass_initialize;
  pass->pass_end = pass_end;
  pass->function_start = function_start;
  pass->function_end = function_end;
  pass->control_start = control_start;
  pass->control_end = control_end;
  pass->instruction_start = instruction_start;
  pass->instruction_end = instruction_end;
  pass->machine_inst_start = machine_inst_start;
  pass->machine_inst_end = machine_inst_end;
  pass->validation = validation;
  pass->dep_list = malloc(sizeof(struct DepList));
  dep_list_init(pass->dep_list);
}

void
pass_add_dep(struct Pass* pass, char* name)
{
  if (!dep_list_grow(pass->dep_list))
    assert(0);
  struct DepList* dep_list = pass->dep_list;
  char** dep_name = &dep_list->data[dep_list->size - 1];
  int str_len = strlen(name);

  *dep_name = malloc(str_len + 1);
  memcpy(*dep_name, name, str_len + 1);
  (*dep_name)[str_len] = '\0';
}

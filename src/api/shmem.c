#include "api.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifdef _WIN32
  #include <windows.h>

  typedef HANDLE shmem_handle;
  typedef HANDLE shmem_mutex;
#else
  #include <unistd.h>
  #include <sys/mman.h>
  #include <sys/stat.h>
  #include <fcntl.h>
  #include <semaphore.h>

  typedef int shmem_handle;
  typedef sem_t* shmem_mutex;
#endif


typedef struct {
  shmem_handle handle;
  char name[256];
  size_t size;
  void* map;
} shmem_object;

typedef struct {
  char name[256];
  size_t size;
} shmem_entry;

typedef struct {
  int refcount;
  size_t size;
  size_t capacity;
  shmem_entry entries[];
} shmem_namespace;

typedef struct {
  shmem_object* handle;
  shmem_mutex mutex;
  shmem_namespace* namespace;
} shmem_container;


shmem_object* shmem_open(const char* name, size_t size) {
  shmem_object* object = malloc(sizeof(shmem_object));
  strcpy(object->name, name);
  object->size = size;

#ifdef _WIN32
  object->handle = CreateFileMapping(
    INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, size, name
  );

  if (object->handle == NULL) goto shmem_open_error;

  object->map = MapViewOfFile(
    object->handle, FILE_MAP_ALL_ACCESS, 0, 0, size
  );

  if (object->map == NULL) {
    CloseHandle(object->handle);
    goto shmem_open_error;
  }
#else
  object->handle = shm_open(name, O_CREAT | O_RDWR, 0666);

  if (object->handle == -1) goto shmem_open_error;

  if (ftruncate(object->handle, size) == -1) {
    close(object->handle);
    goto shmem_open_error;
  }

  object->map = mmap(
    NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, object->handle, 0
  );

  if (object->map == MAP_FAILED) {
    close(object->handle);
    goto shmem_open_error;
  }
#endif

  return object;

shmem_open_error:
  free(object);
  return NULL;
}

void shmem_close(shmem_object* object, bool unregister) {
#ifdef _WIN32
  UnmapViewOfFile(object->map);
  CloseHandle(object->handle);
#else
  munmap(object->map, sizeof(object->size));
  close(object->handle);

  if (unregister)
    shm_unlink(object->name);
#endif

  free(object);
}

shmem_mutex shmem_mutex_new(const char* name) {
  shmem_mutex mutex;

#ifdef _WIN32
  mutex = CreateMutex(NULL, FALSE, name);
  if (mutex == NULL) return NULL;
#else
  mutex = sem_open(name, O_CREAT, 0666, 1);
  if (mutex == SEM_FAILED) return NULL;
#endif

  return mutex;
}

void shmem_mutex_lock(shmem_mutex mutex) {
#ifdef _WIN32
  WaitForSingleObject(mutex, INFINITE);
#else
  sem_wait(mutex);
#endif
}

void shmem_mutex_unlock(shmem_mutex mutex) {
#ifdef _WIN32
  ReleaseMutex(mutex);
#else
  sem_post(mutex);
#endif
}

void shmem_mutex_delete(shmem_mutex mutex, const char* name) {
#ifdef _WIN32
  CloseHandle(mutex);
#else
  sem_close(mutex);
  if (name != NULL) sem_unlink(name);
#endif
}

size_t shmem_container_ns_entries_find(
  shmem_container* container,
  const char* name
) {
  size_t position = -1;

  shmem_mutex_lock(container->mutex);
  for (size_t i=0; i<container->namespace->size; i++) {
    if (strcmp(container->namespace->entries[i].name, name) == 0) {
      position = i;
      break;
    }
  }
  shmem_mutex_unlock(container->mutex);

  return position;
}

bool shmem_container_ns_entries_add(
  shmem_container* container,
  const char* name,
  const char* value,
  size_t value_size
) {
  bool added = false;

  char* ns_name = malloc(strlen(container->handle->name) + strlen(name) + 1);
  sprintf(ns_name, "%s|%s", container->handle->name, name);

  shmem_mutex_lock(container->mutex);
  size_t pos = container->namespace->size;
  if (pos < container->namespace->capacity) {
    shmem_object* object = shmem_open(ns_name, value_size);
    if (object) {
      strcpy(container->namespace->entries[pos].name, name);
      container->namespace->entries[pos].size = value_size;
      memcpy(object->map, value, value_size);
      container->namespace->size++;
      shmem_close(object, false);
      added = true;
    }
  }
  shmem_mutex_unlock(container->mutex);

  free(ns_name);

  return added;
}

bool shmem_container_ns_entries_set(
  shmem_container* container,
  const char* name,
  const char* value,
  size_t value_size
) {
  bool updated = false;
  shmem_object* object = NULL;
  size_t pos = shmem_container_ns_entries_find(container, name);

  shmem_mutex_lock(container->mutex);

  char* ns_name = malloc(strlen(container->handle->name) + strlen(name) + 1);
  sprintf(ns_name, "%s|%s", container->handle->name, name);

  if (pos == -1) {
    pos = container->namespace->size;

    if (pos < container->namespace->capacity) {
      object = shmem_open(ns_name, value_size);
    }
  } else {
    object = shmem_open(ns_name, value_size);
  }

  if (object) {
    strcpy(container->namespace->entries[pos].name, name);
    container->namespace->entries[pos].size = value_size;
    memcpy(object->map, value, value_size);
    container->namespace->size++;
    shmem_close(object, false);
    updated = true;
  }

  free(ns_name);

  shmem_mutex_unlock(container->mutex);


  return updated;
}

char* shmem_container_ns_entries_get(
  shmem_container* container,
  const char* name,
  size_t* data_len
) {
  char* data = NULL;
  *data_len = 0;

  char* ns_name = malloc(strlen(container->handle->name) + strlen(name) + 1);
  sprintf(ns_name, "%s|%s", container->handle->name, name);

  shmem_mutex_lock(container->mutex);
  for (size_t i=0; i<container->namespace->size; i++) {
    if (strcmp(container->namespace->entries[i].name, name) == 0) {
      shmem_object* object = shmem_open(
        ns_name, container->namespace->entries[i].size
      );

      if (object && container->namespace->entries[i].size > 0) {
        data = malloc(container->namespace->entries[i].size);
        memcpy(data, object->map, container->namespace->entries[i].size);
        *data_len = container->namespace->entries[i].size;
        shmem_close(object, false);
      }

      break;
    }
  }
  shmem_mutex_unlock(container->mutex);

  free(ns_name);

  return data;
}

void shmem_container_ns_entries_remove(
  shmem_container* container,
  const char* name
) {
  size_t pos = shmem_container_ns_entries_find(container, name);

  if (pos != -1) {
    shmem_mutex_lock(container->mutex);

    char* ns_name = malloc(strlen(container->handle->name) + strlen(name) + 1);
    sprintf(ns_name, "%s|%s", container->handle->name, name);

    shmem_object* object = shmem_open(
      ns_name, container->namespace->entries[pos].size
    );

    if (object)
      shmem_close(object, true);

    if ((pos + 1) !=  container->namespace->size) {
      memmove(
        container->namespace->entries+pos,
        container->namespace->entries+pos+1,
        sizeof(shmem_entry)
      );
    }

    container->namespace->size--;

    free(ns_name);

    shmem_mutex_unlock(container->mutex);
  }
}

void shmem_container_ns_entries_clear(shmem_container* container) {
  shmem_mutex_lock(container->mutex);
  if (container->namespace->size > 0) {
    for (size_t i=0; i<container->namespace->size; i++) {
      char* ns_name = malloc(
        strlen(container->handle->name)
        + strlen(container->namespace->entries[i].name)
        + 1
      );

      sprintf(
        ns_name, "%s|%s",
        container->handle->name,
        container->namespace->entries[i].name
      );

      shmem_object* object = shmem_open(
        ns_name, container->namespace->entries[i].size
      );

      if (object) {
        shmem_close(object, true);
        strcpy(container->namespace->entries[i].name, "");
        container->namespace->entries[i].size = 0;
      }

      free(ns_name);
    }
    container->namespace->size = 0;
  }
  shmem_mutex_unlock(container->mutex);
}

size_t shmem_container_ns_get_size(shmem_container* container) {
  shmem_mutex_lock(container->mutex);
  size_t size = container->namespace->size;
  shmem_mutex_unlock(container->mutex);
  return size;
}

size_t shmem_container_ns_get_capacity(shmem_container* container) {
  shmem_mutex_lock(container->mutex);
  size_t capacity = container->namespace->capacity;
  shmem_mutex_unlock(container->mutex);
  return capacity;
}

shmem_container* shmem_container_open(const char* namespace, size_t capacity) {
  shmem_container* container = malloc(sizeof(shmem_container));
  size_t ns_size = sizeof(shmem_namespace) + (capacity * sizeof(shmem_entry));

  shmem_object* object = shmem_open(namespace, ns_size);

  if (!object)
    goto shmem_container_open_error;

  container->handle = object;
  container->namespace = object->map;
  container->mutex = shmem_mutex_new(namespace);

  if (!container->mutex) {
    shmem_close(object, false);
    goto shmem_container_open_error;
  }

  shmem_namespace* ns = container->namespace;

  shmem_mutex_lock(container->mutex);
  if (!ns->refcount) {
    ns->size = 0;
    ns->capacity = capacity;
    ns->refcount = 1;
  } else {
    ns->refcount++;
  }
  shmem_mutex_unlock(container->mutex);

  return container;

shmem_container_open_error:
  free(container);
  return NULL;
}

void shmem_container_close(shmem_container* container) {
  shmem_mutex_lock(container->mutex);

  int refcount = container->namespace->refcount--;

  char namespace[256];
  strcpy(namespace, container->handle->name);

  bool unregister = refcount <= 0;

  shmem_close(container->handle, unregister);

  if(unregister) {
    shmem_container_ns_entries_clear(container);
    shmem_mutex_unlock(container->mutex);
    shmem_mutex_delete(container->mutex, namespace);
  } else {
    shmem_mutex_unlock(container->mutex);
    shmem_mutex_delete(container->mutex, NULL);
  }

  free(container);
}


typedef struct {
  shmem_container* container;
} l_shmem_container;

#define L_SHMEM_SELF(L, idx) ( \
  (l_shmem_container*) luaL_checkudata(L, idx, API_TYPE_SHARED_MEMORY) \
)->container


static int f_shmem_open(lua_State* L) {
  const char* namespace = luaL_checkstring(L, 1);
  size_t capacity = luaL_checkinteger(L, 2);
  shmem_container* container = shmem_container_open(namespace, capacity);

  if(!container) {
    lua_pushnil(L);
    lua_pushstring(L, "error initializing the shared memory container");
    return 2;
  }

  l_shmem_container* self = lua_newuserdata(L, sizeof(l_shmem_container));
  self->container = container;
  luaL_setmetatable(L, API_TYPE_SHARED_MEMORY);

  return 1;
}


static int m_shmem_set(lua_State* L) {
  shmem_container* self = L_SHMEM_SELF(L, 1);
  const char* name = luaL_checkstring(L, 2);
  size_t value_len;
  const char* value = luaL_checklstring(L, 3, &value_len);

  lua_pushboolean(
    L,
    shmem_container_ns_entries_set(self, name, value, value_len)
  );

  return 1;
}


static int m_shmem_get(lua_State* L) {
  shmem_container* self = L_SHMEM_SELF(L, 1);
  const char* name = luaL_checkstring(L, 2);

  size_t data_len;
  char* data = shmem_container_ns_entries_get(self, name, &data_len);

  if (data) {
    lua_pushlstring(L, data, data_len);
    free(data);
  }
  else
    lua_pushnil(L);

  return 1;
}


static int m_shmem_remove(lua_State* L) {
  shmem_container* self = L_SHMEM_SELF(L, 1);
  const char* name = luaL_checkstring(L, 2);
  shmem_container_ns_entries_remove(self, name);
  return 0;
}


static int m_shmem_clear(lua_State* L) {
  shmem_container* self = L_SHMEM_SELF(L, 1);
  shmem_container_ns_entries_clear(self);
  return 0;
}


static int m_shmem_size(lua_State* L) {
  shmem_container* self = L_SHMEM_SELF(L, 1);
  lua_pushinteger(L, shmem_container_ns_get_size(self));
  return 1;
}


static int m_shmem_capacity(lua_State* L) {
  shmem_container* self = L_SHMEM_SELF(L, 1);
  lua_pushinteger(L, shmem_container_ns_get_capacity(self));
  return 1;
}


static int mm_shmem_gc(lua_State* L) {
  shmem_container* self = L_SHMEM_SELF(L, 1);
  shmem_container_close(self);
  return 0;
}


static const luaL_Reg shmem_lib[] = {
  { "open",     f_shmem_open     },
  {NULL, NULL}
};

static const luaL_Reg shmem_class[] = {
  { "set",      m_shmem_set      },
  { "get",      m_shmem_get      },
  { "remove",   m_shmem_remove   },
  { "clear",    m_shmem_clear    },
  { "size",     m_shmem_size     },
  { "capacity", m_shmem_capacity },
  { "__gc",     mm_shmem_gc      },
  /* TODO: Implement __pairs */
  {NULL, NULL}
};


int luaopen_shmem(lua_State* L) {
  luaL_newmetatable(L, API_TYPE_SHARED_MEMORY);
  luaL_setfuncs(L, shmem_class, 0);
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");

  luaL_newlib(L, shmem_lib);
  return 1;
}

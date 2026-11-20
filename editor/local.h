#ifndef EDITOR_LOCAL_H
#define EDITOR_LOCAL_H

#include <windows.h>

/* Workspace comes from the user, not a project. Names within it come from
   vc_picture_path; project destinations are additionally restricted to leaves. */
const char *local_init(const char *argument);
const char *local_read(const char *relative, DWORD limit, char **data, DWORD *size);
const char *local_save(const char *leaf, const char *data, DWORD size, int replace);
void local_close(void);

#endif

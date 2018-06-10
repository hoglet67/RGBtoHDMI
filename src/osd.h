#ifndef OSD_H
#define OSD_H

void osd_init();
void osd_clear();
void osd_set(char *text);
void osd_update(uint32_t *osd_base, int bytes_per_line);
int  osd_active();

#endif

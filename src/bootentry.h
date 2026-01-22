#ifndef __BOOTENTRY_H
#define __BOOTENTRY_H

int bootentry_check(void);
int board_check_double_reset(void);

// Returns 1 if bootloader was entered via command (REQUEST_CANBOOT)
int bootentry_is_commanded(void);

#endif // bootentry.h

#ifndef DUMMY_IOCTL_H
#define DUMMY_IOCTL_H

#define DUMMY_MAGIC     'D'
#define CLEAR_SEQ_NO    0x01
#define RESIZE_SEQ_NO   0x02
#define GETSIZE_SEQ_NO  0x03

#define DUMMY_CLEAR     _IO(DUMMY_MAGIC, CLEAR_SEQ_NO)
#define DUMMY_RESIZE    _IOW(DUMMY_MAGIC, RESIZE_SEQ_NO, unsigned char)
#define DUMMY_GETSIZE   _IOR(DUMMY_MAGIC, GETSIZE_SEQ_NO, int *)

#endif
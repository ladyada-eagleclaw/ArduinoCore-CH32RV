/* Preserve the first 32 KiB for TinyUF2. */
ENTRY(_start)
MEMORY
{
  FLASH (rx) : ORIGIN = 0x00008000, LENGTH = 491520
  RAM (xrw) : ORIGIN = 0x20000000, LENGTH = 204800
}
INCLUDE sections.ld

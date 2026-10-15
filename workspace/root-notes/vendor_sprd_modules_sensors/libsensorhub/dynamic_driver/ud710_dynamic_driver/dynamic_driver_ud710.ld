MEMORY
{
/* sensor dynamic driver store and run on sram, the space size for sensor
 * dynamic driver is 20KB. the start address of SC9863A/UMS312/UD710 dynamic
 * driver is 0x3B000, and UMS512 is 0x49000
 */
	RAM (rwx) : ORIGIN = 0x3B000, LENGTH = 20K
}

SECTIONS
{
	/* . = 0x3b000;*/
	sensor_driver_interface : {
		KEEP(*(sensor_driver_interface));
	} > RAM

	. = 0x3B100;

	.text : {
		__text_start = .;
		*(.text*);
		*(.rodata*);
		*(.constdata*);
		__text_end = .;
	} > RAM

	.data : {
		*(.data*);
	} > RAM

	.bss : {
		__bss_start = .;
		*(.bss*);
		*(COMMON);
		__bss_end = .;
	} > RAM
}
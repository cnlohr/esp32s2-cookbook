uint32_t * ulp_count = 0x50000060;

void rv32_main()
{
	*ulp_count = 0x55555555;
}

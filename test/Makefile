all : asm

asm : test_256 test_512 test_1k test_2k test_4k test_8k test_16k test_32k test_64k test_128k test_256k test_512k test_1024k test_2048k test_trans

clean :
	rm gpu_fft_*.hex *.strip

.SECONDARY :

shader_%.strip : shader_%.hex
	sed 's/\s*\/\/.*//' $< >$@

test_% : gpu_fft_%.hex shader_%.strip
	diff $^

gpu_fft_%.hex : gpu_fft_%.qasm gpu_fft.qinc gpu_fft_ex.qinc ../bin/vc4asm
	../bin/vc4asm -V -c $@ ../share/vc4.qinc $<

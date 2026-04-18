#include <cstdio>
#include <string>
#include <cstdint>
#include <cstdlib>
#include <cstring>

/* ./test_usb_sb <device> <sd_blocks> */

const size_t sd_block_size = 512ULL;

const size_t noise_size = 65536;
uint8_t noise[noise_size * 2] = { 0 };

const size_t buf_size = 512 * 1024ULL;	// maximum transaction size
const size_t buf_blocks = buf_size / sd_block_size;

uint8_t buf[buf_size] = { 0 };

size_t nfails = 0;

static void fill_noise(uint8_t* dst, size_t len)
{
	for (auto i = 0ull; i < len; i += noise_size)
	{
		auto noise_start = rand() & (noise_size - 1) & ~7ULL;
		memcpy(&dst[i], &noise[noise_start], noise_size);
	}
}

int main(int argc, char *argv[])
{
    if(argc != 3)
    {
        printf("%s <device> <sd_blocks>\n", argv[0]);
        return -1;
    }

    auto device = argv[1];
    auto sd_blocks = (size_t)std::stoi(argv[2]);
    if(sd_blocks <= 0)
    {
        printf("%s <device> <sd_blocks>\n", argv[0]);
        return -1;
    }

    auto sd_size = sd_blocks * sd_block_size;
    auto sd = new uint8_t[sd_size];

	/* Generate some noise */
	for (auto i = 0ull; i < noise_size * 2; i++)
	{
		noise[i] = (uint8_t)rand();
	}

	fill_noise(sd, sd_size);
	printf("prepping done\n");

    // write out all the blocks
    auto fd = fopen(device, "w+b");
    if(!fd)
    {
        printf("failed to open %s\n", device);
        return -1;
    }

    setvbuf(fd, nullptr, _IONBF, 0);

    auto nw = fwrite(sd, 1, sd_size, fd);
    fflush(fd);

    if(nw != sd_size)
    {
        printf("failed to write to sd: %zu\n", nw);
        return -1;
    }
    printf("sd written out\n");

	/* Now run test reads/writes at random offsets */
    auto ntrials = sd_blocks * 4;
	for (size_t i = 0u; i < ntrials; i++)
	{
		auto is_read = (rand() & 1) != 0;
		auto nblocks = rand() % buf_blocks;
        if(!nblocks) nblocks = 1;
		auto block_addr = rand() % sd_blocks;
		if (block_addr + nblocks > sd_blocks)
		{
			nblocks = sd_blocks - block_addr;
		}

		if (!is_read)
		{
			fill_noise(buf, nblocks * sd_block_size);
            memcpy(&sd[block_addr * sd_block_size], buf, nblocks * sd_block_size);
		}

        if(fseek(fd, block_addr * sd_block_size, SEEK_SET))
        {
            printf("fseek fail %d\n", errno);
            return -1;
        }
        auto nwr = is_read ? fread(buf, sd_block_size, nblocks, fd) :
            fwrite(buf, sd_block_size, nblocks, fd);
        fflush(fd);
        if(nwr != nblocks)
        {
            printf("%zu: FAIL to %s %zu blocks @ %zu: %zu (%d: %s)\n", i, 
                is_read ? "read" : "write",                
                nblocks, block_addr,
                nwr, errno, strerror(errno));
            return -1;
        }

        if (is_read && memcmp(buf, &sd[block_addr * sd_block_size],
			nblocks * sd_block_size))
		{
			printf("FAIL: %zu\n", i);
			nfails++;
		}

		if (((i % 1000) == 0) && (i != 0))
		{
			printf("TRIAL: %zu/%zu\n", i, ntrials);
		}
	}

	printf("FINISHED: %zu fails, %zu trials\n", nfails, ntrials);

    /* Now read back the whole device to check */
    auto sd2 = new uint8_t[sd_size];
    fseek(fd, 0, SEEK_SET);
    fread(sd2, 1, sd_size, fd);

    if(memcmp(sd, sd2, sd_size))
    {
        printf("buffers differ\n");
        nfails++;
    }

	return nfails;
}

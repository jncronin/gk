#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>

/* cache size is 64M, so create a backing SD much larger than this */
const size_t sd_size = 1 * 1024 * 1024 * 1024ULL;
const size_t sd_block_size = 512ULL;
const size_t sd_blocks = sd_size / sd_block_size;
uint8_t sd[sd_size] = { 0 };

const size_t noise_size = 65536;
uint8_t noise[noise_size * 2] = { 0 };

const size_t ntrials = 50000u;
const size_t buf_size = 512 * 1024ULL;	// maximum transaction size
const size_t buf_blocks = buf_size / sd_block_size;

uint8_t buf[buf_size] = { 0 };

size_t nfails = 0;

int sd_cache_init();
int sd_transfer(uint32_t block_start, uint32_t block_count,
	void* mem_address, bool is_read);

static void fill_noise(uint8_t* dst, size_t len)
{
	for (auto i = 0ull; i < len; i += noise_size)
	{
		auto noise_start = rand() & (noise_size - 1) & ~7ULL;
		memcpy(&dst[i], &noise[noise_start], noise_size);
	}
}

int main()
{
	printf("prepping SD with random noise\n");

	/* First, fill 2 big blocks */
	for (auto i = 0ull; i < noise_size * 2; i++)
	{
		noise[i] = (uint8_t)rand();
	}

	/* Then copy big block by big block with random offset into noise */
	fill_noise(sd, sd_size);
	printf("prepping done\n");

	/* init sd cache */
	if (sd_cache_init() != 0)
	{
		printf("sd_cache_init failed\n");
		return -1;
	}
	printf("sd cache init done\n");


	/* Now run test reads/writes at random offsets */
	for (size_t i = 0u; i < ntrials; i++)
	{
		auto is_read = (rand() & 1) != 0;
		auto nblocks = rand() % buf_blocks;
		auto block_addr = rand() % sd_blocks;
		if (block_addr + nblocks > sd_blocks)
		{
			nblocks = sd_blocks - block_addr;
		}

		if (!is_read)
		{
			fill_noise(buf, nblocks * sd_block_size);
		}

		sd_transfer(block_addr, nblocks, buf, is_read);
		if (memcmp(buf, &sd[block_addr * sd_block_size],
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

	return nfails;
}

int sd_perform_transfer(uint32_t block_start, uint32_t block_count,
	void* mem_address, bool is_read, int nretries)
{
	if (is_read)
	{
		memcpy(mem_address, &sd[block_start * sd_block_size], block_count * sd_block_size);
	}
	else
	{
		memcpy(&sd[block_start * sd_block_size], mem_address, block_count * sd_block_size);
	}

	return 0;
}

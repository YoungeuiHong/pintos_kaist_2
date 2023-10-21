/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};


static struct bitmap* swap_bitmap;
/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */

	swap_disk = disk_get(1,1);
	swap_bitmap = bitmap_create(200);
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	printf("anon_swap_in\n");
	struct anon_page *anon_page = &page->anon;
	disk_sector_t sec_no = anon_page->disk_sec;
	
	disk_read (swap_disk, sec_no, kva);
	anon_page->disk_sec = NULL;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	printf("anon_swap_out\n");
	struct anon_page *anon_page = &page->anon;
	
	// 비트맵에서 빈 칸 찾아오고 거기에 쓰기
	// disk_sector_t sec_no;
	size_t sec_no = bitmap_scan(swap_bitmap, 0, 1, false);
	disk_write (swap_disk, sec_no, page->frame->kva);
	anon_page->disk_sec = sec_no;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;

	struct frame *f = page->frame;
	if (f != NULL)
	{
		pml4_clear_page(thread_current()->pml4, page->va);
		list_remove(&f->list_elem);
		palloc_free_page(f->kva);
		free(f);
	}
}

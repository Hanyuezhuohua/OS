
void _buddy_return_page(page_t* page) {
    // TODO: make merge here
    // Suggested: 4 LoCs
    page->flags = BD_PAGE_FREE;
    bd_lists[page->orders].nr_free++;
    list_add(&page->list_node, &bd_lists[page->orders].list_head);
}

void _buddy_get_specific_page(page_t* page) {
    page->flags = BD_PAGE_IN_USE;
    bd_lists[page->orders].nr_free--;
    list_del(&page->list_node);
}

void _buddy_clear_flag(page_t* page) { page->flags = 0; }

uint64 _buddy_get_page_idx(page_t* page) {
    return (uint64) (page - bd_meta.first_meta_page);
}

uint64 _buddy_get_area_idx(void* head) { 
    return (uint64) ((uint64) head - bd_meta.data_head) / PGSIZE; 
}

page_t* _buddy_idx_get_page(uint64 idx) {
    return bd_meta.first_meta_page + idx;
}

page_t* _buddy_get_buddy_page(page_t* page) {
    // Suggested: 2 LoCs
    uint64 buddy_idx = (((page - bd_meta.first_meta_page)) ^ (1 << page->orders));
    return bd_meta.first_meta_page + buddy_idx ;

}

void _buddy_split_page(page_t *original_page, uint64 target_order){
    // Suggested: 5 LoCs
    while(original_page->orders > target_order){
        original_page->orders--;
        list_add(&original_page[(1U << original_page->orders )].list_node, &bd_lists[original_page->orders].list_head);
        bd_lists[original_page->orders].nr_free++;
        original_page[1U << original_page->orders].orders = original_page->orders;
        original_page[1U << original_page->orders].flags = BD_PAGE_FREE;
    }
    original_page->flags = BD_PAGE_IN_USE;
}

page_t *_buddy_alloc_page(uint64 order){
    // Suggested: 13 LoCs
    uint64 cur_order;
    for(cur_order = order; cur_order < bd_max_size; ++cur_order){
        if(list_empty(&bd_lists[cur_order].list_head)) continue;
        page_t* page_alloc = list_entry(bd_lists[cur_order].list_head.next, page_t, list_node);
        list_del(&page_alloc->list_node);
        bd_lists[cur_order].nr_free--;
        _buddy_split_page(page_alloc, order);
        return page_alloc;  
    }
    return NULL;
}

void buddy_free_page(page_t* page) {
    page_t* current_page = page;
    for (; current_page->orders < bd_max_size - 1; ++current_page->orders) {
        page_t* buddy_page = _buddy_get_buddy_page(current_page);
        if ((!((buddy_page->flags & BD_PAGE_FREE) && current_page->orders == buddy_page->orders))) {
            break;
        }
        if(_buddy_get_page_idx(current_page) > _buddy_get_page_idx(buddy_page)){
            _buddy_get_specific_page(buddy_page);
            _buddy_clear_flag(current_page);
            current_page = buddy_page;
        }
        else{
            _buddy_get_specific_page(buddy_page);
            _buddy_clear_flag(buddy_page);
        }
    }
    _buddy_return_page(current_page);
}
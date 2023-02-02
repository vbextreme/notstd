#include <notstd/rbtree.h>

#define RBT_BLACK   0
#define RBT_RED     1
#define RBT_RAINBOW 2

struct rbtNode{
	struct rbtNode* parent;
	struct rbtNode* left;
	struct rbtNode* right;
	void* data;
	int color;
};

struct rbtree{
	rbtNode_t* root;
	cmp_f cmp;
	size_t count;
};

__private void rbt_leftrotate(rbtNode_t** root, rbtNode_t* p){
    if( p->right == NULL ) return;
    rbtNode_t* y = p->right;
    if( y->left ){
        p->right = y->left;
        y->left->parent = p;
    }
    else{
        p->right=NULL;
	}

	if( p->parent ){
        y->parent = p->parent;
        if( p == p->parent->left ) p->parent->left = y;
        else p->parent->right = y;
	}
	else{
        *root = y;
		y->parent = NULL;
	}

    y->left = p;
    p->parent = y;
}

__private void rbt_rightrotate(rbtNode_t** root, rbtNode_t* p){
    if( p->left == NULL) return ;

    rbtNode_t* y = p->left;
    if( y->right ){
        p->left = y->right;
        y->right->parent = p;
    }
    else{
        p->left = NULL;
	}

	if( p->parent ){
		y->parent = p->parent;
		if( p == p->parent->left ) p->parent->left = y;
		else p->parent->right = y;
	}
	else{
        *root = y;
		y->parent = NULL;
	}
    
	y->right = p;
    p->parent = y;
}

__private void rbt_insertfix(rbtNode_t** root, rbtNode_t *page){
    rbtNode_t* u;
    if( *root == page){
        page->color = RBT_BLACK;
        return;
    }
	iassert(page->parent);
    while( page->parent && page->parent->color == RBT_RED){
        rbtNode_t* g = page->parent->parent;
		iassert(g);
        if( g->left == page->parent ){
            if( g->right ){
                u = g->right;
				if( u->color == RBT_RED ){
                    page->parent->color = RBT_BLACK;
                    u->color = RBT_BLACK;
                    g->color = RBT_RED;
                    page = g;
                }
				else{
					break;
				}
            }
            else{
                if( page->parent->right == page ){
                    page = page->parent;
                    rbt_leftrotate(root, page);
                }
                page->parent->color = RBT_BLACK;
                g->color = RBT_RED;
                rbt_rightrotate(root, g);
            }
        }
        else{
            if( g->left ){
                u = g->left;
				if( u->color == RBT_RED ){
                    page->parent->color = RBT_BLACK;
                    u->color = RBT_BLACK;
                    g->color = RBT_RED;
                    page = g;
                }
				else{
					break;
				}
            }
            else{
                if( page->parent->left == page ){
                    page = page->parent;
                    rbt_rightrotate(root, page);
                }
                page->parent->color = RBT_BLACK;
                g->color = RBT_RED;
                rbt_leftrotate(root, g);
            }
        }
        (*root)->color = RBT_BLACK;
    }
}

rbtNode_t* rbtree_insert(rbtree_t* rbt, rbtNode_t* page){
	if( page->color != RBT_RAINBOW ) return page;

	page->color = RBT_RED;
    rbtNode_t* p = rbt->root;
    rbtNode_t* q = NULL;

    if( !rbt->root ){ 
		rbt->root = page;
	}
    else{
        while( p != NULL ){
            q = p;
            if( rbt->cmp(p->data, page->data) < 0 )
                p = p->right;
            else
                p = p->left;
        }
        page->parent = q;
        if( rbt->cmp(q->data, page->data) < 0 )
            q->right = page;
        else
            q->left = page;
    }
    rbt_insertfix(&rbt->root, page);
	++rbt->count;
	return page;
}

__private void rbt_removefix(rbtNode_t** root, rbtNode_t* p){
	iassert(p);
    rbtNode_t* s;
    while( p != *root && p->color == RBT_BLACK ){
        if( p->parent->left == p ){
            s = p->parent->right;
            if( s->color == RBT_RED ){
                s->color = RBT_BLACK;
                p->parent->color = RBT_RED;
                rbt_leftrotate(root, p->parent);
                s = p->parent->right;
            }
            if( s->right->color == RBT_BLACK && s->left->color == RBT_BLACK ){
                s->color = RBT_RED;
                p = p->parent;
            }
            else{
                if( s->right->color == RBT_BLACK ){
                    s->left->color = RBT_BLACK;
                    s->color = RBT_RED;
                    rbt_rightrotate(root, s);
                    s = p->parent->right;
                }
                s->color = p->parent->color;
                p->parent->color = RBT_BLACK;
                s->right->color = RBT_BLACK;
                rbt_leftrotate(root, p->parent);
                p = *root;
            }
        }
        else{
            s = p->parent->left;
            if( s->color == RBT_RED ){
                s->color=RBT_BLACK;
                p->parent->color = RBT_RED;
                rbt_rightrotate(root, p->parent);
                s = p->parent->left;
            }
            if( s->left->color == RBT_BLACK && s->right->color == RBT_BLACK){
                s->color = RBT_RED;
                p = p->parent;
            }
            else{
                if(s->left->color == RBT_BLACK){
                    s->right->color = RBT_BLACK;
                    s->color = RBT_RED;
                    rbt_leftrotate(root, s);
                    s = p->parent->left;
                }
                s->color = p->parent->color;
                p->parent->color = RBT_BLACK;
                s->left->color = RBT_BLACK;
                rbt_rightrotate(root, p->parent);
                p = *root;
            }
        }

        p->color=RBT_BLACK;
        (*root)->color = RBT_BLACK;
    }
}

__private rbtNode_t* rbt_successor(rbtNode_t* p){
    rbtNode_t* y = NULL;
    if( p->left ){
        y = p->left;
        while( y->right ) y = y->right;
    }
    else{
        y = p->right;
        while( y->left ) y = y->left;
    }
    return y;
}

rbtNode_t* rbtree_remove(rbtree_t* rbt, rbtNode_t* p){
	if( p->color == RBT_RAINBOW ) return p;
	if( !rbt->root || !p ) return NULL;
	--rbt->count;
	if( (rbt->root) == p && (rbt->root)->left == NULL && (rbt->root)->right == NULL ){
		p->parent = p->left = p->right = NULL;
		rbt->root = NULL;
		p->color = RBT_RAINBOW;
		return p;
	}

    rbtNode_t* y = NULL;
    rbtNode_t* q = NULL;

	if( p->left == NULL || p->right==NULL ) y = p;
	else y = rbt_successor(p);

	if( y->left ) q = y->left;
	else if( y->right ) q = y->right;

	if( q ) q->parent = y->parent;

	if( !y->parent ) rbt->root = q;
	else{
        if( y == y->parent->left ) y->parent->left = q;
        else y->parent->right = q;
    }

    if( y != p ){
		p->color = y->color;
		if( p->parent ){
			if( p->parent->left == p ) p->parent->left = y;
			else if( p->parent->right == p ) p->parent->right = y;
		}
		else{
			rbt->root = y;
		}
		if( y->parent ){
			if( y->parent->left == y ) y->parent->left = p;
			else if( y->parent->right == y ) y->parent->right = p;
		}
		else{
			rbt->root = p;
		}
		swap(p->parent, y->parent);
		swap(p->left, y->left);
		swap(p->right, y->right);
	}
	if( q && y->color == RBT_BLACK ) rbt_removefix(&rbt->root, q);

	if( p->parent ){
		if( p->parent->left == p ) p->parent->left = NULL;
		else if( p->parent->right == p ) p->parent->right = NULL;
	}
	p->parent = p->left = p->right = NULL;
	p->color = RBT_RAINBOW;
	return p;
}

rbtNode_t* rbtree_find(rbtree_t* rbt, const void* key){
	rbtNode_t* p = rbt->root;
	int cmp;
    while( p && (cmp=rbt->cmp(p->data, key)) ){
        p = cmp < 0 ? p->right : p->left;
    }
	return p;
}

rbtNode_t* rbtree_find_best(rbtree_t* rbt, const void* key){
	rbtNode_t* p = rbt->root;
	while( p && rbt->cmp(p->data, key) < 0 ){
		p = p->right;
		while( p && p->left && rbt->cmp(p->left->data, key) >= 0 ) p = p->left;
    }
	return p;
}

rbtree_t* rbtree_new(cmp_f fn){
	rbtree_t* t = NEW(rbtree_t);
	t->root  = NULL;
	t->cmp   = fn;
	t->count = 0;
	return t;
}

rbtree_t* rbtree_cmp(rbtree_t* t, cmp_f fn){
	t->cmp = fn;
	return t;
}

rbtNode_t* rbtree_node_new(void* data){
	rbtNode_t* n = NEW(rbtNode_t);
	n->color  = RBT_RAINBOW;
	n->data   = data; 
	n->parent = n->left = n->right = NULL;
	return n;
}

void* rbtree_node_data(rbtNode_t* n){
	return n->data;
}

rbtNode_t* rbtree_node_data_set(rbtNode_t* n, void* data){
	n->data = data;
	return n;
}

rbtNode_t* rbtree_node_root(rbtree_t* t){
	return t->root;
}

rbtNode_t* rbtree_node_left(rbtNode_t* node){
	if( node->color == RBT_RAINBOW ) return NULL;
	return node->left;
}

rbtNode_t* rbtree_node_right(rbtNode_t* node){
	if( node->color == RBT_RAINBOW ) return NULL;
	return node->right;
}

rbtNode_t* rbtree_node_parent(rbtNode_t* node){
	if( node->color == RBT_RAINBOW ) return NULL;
	return node->parent;
}

void map_rbtree_inorder(rbtNode_t* n, rbtMap_f fn, void* arg){
	if( n == NULL || n->color == RBT_RAINBOW ) return;
	map_rbtree_inorder(n->left, fn, arg);
	if( fn(n->data, arg) ) return;
	map_rbtree_inorder(n->right, fn, arg);
}

void map_rbtree_preorder(rbtNode_t* n, rbtMap_f fn, void* arg){
	if( n == NULL || n->color == RBT_RAINBOW ) return;
	if( fn(n->data, arg) ) return;
	map_rbtree_inorder(n->left, fn, arg);
	map_rbtree_inorder(n->right, fn, arg);
}

void map_rbtree_postorder(rbtNode_t* n, rbtMap_f fn, void* arg){
	if( n == NULL || n->color == RBT_RAINBOW ) return;
	map_rbtree_inorder(n->left, fn, arg);
	map_rbtree_inorder(n->right, fn, arg);
	fn(n->data, arg);
}

size_t rbtree_count(rbtree_t* t){
	return t->count;
}

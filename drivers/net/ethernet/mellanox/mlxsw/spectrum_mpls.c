/*
 * drivers/net/ethernet/mellanox/mlxsw/spectrum_mr.c
 * Copyright (c) 2017 Mellanox Technologies. All rights reserved.
 * Copyright (c) 2017 Yotam Gigi <yotamg@mellanox.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the names of the copyright holders nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/rhashtable.h>

#include "spectrum_mpls.h"

struct mlxsw_sp_fib_mpls_entry {
	struct mlxsw_sp_fib_entry common;
	struct rhash_head ht_node;
	u32 label;
};

static const struct rhashtable_params mlxsw_sp_mpls_route_ht_params = {
	.key_len = sizeof(u32),
	.key_offset = offsetof(struct mlxsw_sp_fib_mpls_entry, label),
	.head_offset = offsetof(struct mlxsw_sp_fib_mpls_entry, ht_node),
	.automatic_shrinking = true,
};

static int mlxsw_sp_nexthop4_init(struct mlxsw_sp *mlxsw_sp,
				  struct mlxsw_sp_nexthop_group *nh_grp,
				  struct mlxsw_sp_nexthop *nh,
				  struct mpls_nh *mpls_nh)
{

}

struct mpls_nh *
mlxsw_sp_nexthop_mpls_get_by_index(struct mpls_route *rt, int i)
{
	struct mpls_nh *mpls_nhi = NULL;

	if (i >= rt->rt_nhn)
		return NULL;

	for_nexthops(rt) {
		mpls_nh = nh;
		if (nhsel < i)
			continue;
		break;
	} endfor_nexthops(rt);

	return mpls_nh;
}

static struct mlxsw_sp_nexthop_group *
mlxsw_sp_nexthop_mpls_group_create(struct mlxsw_sp *mlxsw_sp,
				   struct mpls_route *rt)
{
	struct mlxsw_sp_nexthop_group *nh_grp;
	struct mlxsw_sp_nexthop *nh;
	size_t alloc_size;
	int i;
	int err;

	alloc_size = sizeof(*nh_grp) +
		     rt->rt_nhn * sizeof(struct mlxsw_sp_nexthop);
	nh_grp = kzalloc(alloc_size, GFP_KERNEL);
	if (!nh_grp)
		return ERR_PTR(-ENOMEM);
	nh_grp->priv = rt;
	INIT_LIST_HEAD(&nh_grp->fib_list);
	nh_grp->neigh_tbl = &arp_tbl;

	nh_grp->gateway = true;
	nh_grp->count = rt->rt_nhn;
//	fib_info_hold(fi);
	for (i = 0; i < nh_grp->count; i++) {
		struct mpls_nh *mpls_nh;

		nh = &nh_grp->nexthops[i];
		mpls_nh = mlxsw_sp_nexthop_mpls_get_by_index(rt, i);
		err = mlxsw_sp_nexthop_mpls_init(mlxsw_sp, nh_grp, nh, mpls_nh);
		if (err)
			goto err_nexthop_mpls_init;
	}
	mlxsw_sp_nexthop_group_refresh(mlxsw_sp, nh_grp);
	return nh_grp;

err_nexthop_mpls_init:
	for (i--; i >= 0; i--) {
		nh = &nh_grp->nexthops[i];
		mlxsw_sp_nexthop_mpls_fini(mlxsw_sp, nh);
	}
//	fib_info_put(fi);
	kfree(nh_grp);
	return ERR_PTR(err);
	return NULL;
}

static void
mlxsw_sp_nexthop_mpls_group_destroy(struct mlxsw_sp *mlxsw_sp,
				    struct mlxsw_sp_nexthop_group *nh_grp)
{
	struct mlxsw_sp_nexthop *nh;
	int i;

//	mlxsw_sp_nexthop_group_remove(mlxsw_sp, nh_grp);
	for (i = 0; i < nh_grp->count; i++) {
		nh = &nh_grp->nexthops[i];
//		mlxsw_sp_nexthop4_fini(mlxsw_sp, nh);
	}
	mlxsw_sp_nexthop_group_refresh(mlxsw_sp, nh_grp);
	WARN_ON_ONCE(nh_grp->adj_index_valid);
//	fib_info_put(mlxsw_sp_nexthop4_group_fi(nh_grp));
	kfree(nh_grp);
}

static int
mlxsw_sp_nexthop_mpls_group_get(struct mlxsw_sp *mlxsw_sp,
				struct mlxsw_sp_fib_entry *fib_entry,
				struct mpls_route *rt)
{
	struct mlxsw_sp_nexthop_group *nh_grp;

	nh_grp = mlxsw_sp_nexthop_mpls_group_create(mlxsw_sp, rt);
	if (IS_ERR(nh_grp))
		return PTR_ERR(nh_grp);

	list_add_tail(&fib_entry->nexthop_group_node, &nh_grp->fib_list);
	fib_entry->nh_group = nh_grp;
	return 0;
}

static void
mlxsw_sp_nexthop_mpls_group_put(struct mlxsw_sp *mlxsw_sp,
				struct mlxsw_sp_fib_entry *fib_entry)
{
	struct mlxsw_sp_nexthop_group *nh_grp = fib_entry->nh_group;

	list_del(&fib_entry->nexthop_group_node);
	if (!list_empty(&nh_grp->fib_list))
		return;
	mlxsw_sp_nexthop_mpls_group_destroy(mlxsw_sp, nh_grp);
}

static struct mlxsw_sp_fib_mpls_entry *
mlxsw_sp_mpls_route_create(struct mlxsw_sp *mlxsw_sp,
			   struct mpls_route *rt, u32 label)
{
	struct mlxsw_sp_fib_mpls_entry *fib_mpls_entry;
	struct mlxsw_sp_fib_entry *fib_entry;
	int err;

	fib_mpls_entry = kzalloc(sizeof(*fib_mpls_entry), GFP_KERNEL);
	if (!fib_mpls_entry)
		return ERR_PTR(-ENOMEM);

	fib_entry = &fib_mpls_entry->common;
	fib_entry->type = MLXSW_SP_FIB_ENTRY_TYPE_REMOTE;

	err = mlxsw_sp_nexthop_mpls_group_get(mlxsw_sp, fib_entry, rt);
	if (err)
		goto err_nexthop_group_create;

	fib_mpls_entry->label = label;
	err = rhashtable_insert_fast(&mlxsw_sp->router->mpls_ht,
				     &fib_mpls_entry->ht_node,
				     mlxsw_sp_mpls_route_ht_params);
	if (err)
		goto err_rhashtable_insert;

	return fib_mpls_entry;

err_rhashtable_insert:
	mlxsw_sp_nexthop_mpls_group_put(mlxsw_sp, fib_entry);
err_nexthop_group_create:
	kfree(fib_mpls_entry);
	return ERR_PTR(err);
}

static void
mlxsw_sp_fib_mpls_entry_destroy(struct mlxsw_sp *mlxsw_sp,
				struct mlxsw_sp_fib_mpls_entry *fib_mpls_entry)
{
	mlxsw_sp_nexthop_mpls_group_put(mlxsw_sp, &fib_mpls_entry->common);
	kfree(fib_mpls_entry);
}

void mlxsw_sp_router_mpls_init(struct mlxsw_sp *mlxsw_sp)
{
	char mpgcr_pl[MLXSW_REG_MPGCR_LEN];
	int err;

	mlxsw_reg_mpgcr_pack(mpgcr_pl, 10, 1000);
	mlxsw_reg_write(mlxsw_sp->core, MLXSW_REG(mpgcr), mpgcr_pl);

	err = rhashtable_init(&mlxsw_sp->router->mpls_ht,
			      &mlxsw_sp_mpls_route_ht_params);
	return err;
}


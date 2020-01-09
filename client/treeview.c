#include "lin.h"
#include "treeview.h"

// 用户自己头像相关
extern GtkWidget *button_image, *button_preview;
extern GtkWidget *image_user, *image_user_old, *image_user_new;
extern gchar *str_image_user, *str_image_user_new;

extern char my_id[15]; // 自己的 id

extern GData *widget_chat_dlg;// 聊天窗口集, 用于获取已经建立的聊天窗口
GArray *array_item_num = NULL; // 本根节点下的 child item 个数
GArray *array_face = NULL; // 储存用户的头像数组
int my_indices_num; // 自己所在的 tree view 的目录级, 用于确定群聊窗口是否是自己的部门下的
gboolean init_me_first = FALSE; // 初始化自己相关的头像信息

GtkWidget *create_tree_view()
{
	GtkWidget *treeview;

	// 创建 tree view
	treeview = gtk_tree_view_new();
	setup_tree_view(treeview);
	g_signal_connect (G_OBJECT (treeview), "row-activated",
			G_CALLBACK (row_activated), NULL);

	return treeview;
}

void setup_tree_view(GtkWidget *treeview)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	/* 创建 tree view 的 column，带有icon 和 text. */
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, "好友列表");

	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_set_attributes(column, renderer, "pixbuf", ICON, NULL);

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer, "text", NAME);
	// 增加文字颜色字段
	gtk_tree_view_column_add_attribute(column, renderer, "foreground", COLOR);

	gtk_tree_view_append_column(GTK_TREE_VIEW (treeview), column);

	// 仅文本 column
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("IP 地址", renderer,
			"text", IP, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW (treeview), column);
}

void setup_tree_view_model(GtkWidget *treeview, GArray *array_user,
		gchar *my_id)
{
	if (array_face != NULL && array_face->len != 0)
		g_array_remove_range(array_face, 0, array_face->len);
	else
		array_face = g_array_new(FALSE, TRUE, sizeof(int));
	if (array_item_num != NULL && array_item_num->len != 0)
		g_array_remove_range(array_item_num, 0, array_item_num->len);
	else
		array_item_num = g_array_new(FALSE, TRUE, sizeof(int));

	GtkTreeStore *store;
	GtkTreeIter iter, child;
	GtkTreePath *path_expand = NULL; // 只展开和自己相关部门的 tree view
	guint i, j, on, all;
	GdkPixbuf *pixbuf;

	store = gtk_tree_store_new(N_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING,
			G_TYPE_STRING, G_TYPE_STRING);
	gtk_tree_store_clear(store);

	if (array_user != NULL)
		for (i = 0; i < array_user->len; i++)
		{
			Tree_Item *pItem = &g_array_index(array_user, Tree_Item, i);

			if (pItem->item_type == PARENT_ITEM)
			{
				on = all = 0;
				for (j = i + 1; j < array_user->len; j++)
				{
					Tree_Item *pItem = &g_array_index(array_user, Tree_Item, j);

					if (pItem->item_type != PARENT_ITEM)
					{
						if (pItem->online)
							on++;
						all++;
					}
					else
						break;
				}

				// 统计本部门人数总和
				g_array_append_val(array_item_num, all);

				// 显示总人数和在线人数
				gchar *numinfo = g_strdup_printf("   [%d/%d]", on - 1, all - 1);
				gchar *gstr = g_strconcat(pItem->id, numinfo, NULL);

				// 增加根节点
				gtk_tree_store_append(store, &iter, NULL);
				gtk_tree_store_set(store, &iter, NAME, gstr, -1);
			}
			else
			{
				// 增加用户信息子节点
				gchar *gstrface = g_strdup_printf("./Image/newface/%d.bmp",
						pItem->face);
				pixbuf = gdk_pixbuf_new_from_file(gstrface, NULL);

				gchar *gstr = g_strconcat(pItem->name, "\n", pItem->id, NULL);

				gtk_tree_store_append(store, &child, &iter);

				if (pItem->ip_addr.s_addr != 0)
					gtk_tree_store_set(store, &child, ICON, pixbuf, NAME, gstr,
							IP, inet_ntoa(pItem->ip_addr), COLOR,
							pItem->online ? "red" : "grey", -1);
				else
					gtk_tree_store_set(store, &child, ICON, pixbuf, NAME, gstr,
							IP, "", COLOR, pItem->online ? "red" : "grey", -1);

				// 自己的 id 和 item 放在第一条
				if (strcmp(pItem->id, my_id) == 0)
				{
					path_expand = gtk_tree_model_get_path(
							GTK_TREE_MODEL (store), &child);
					int *idc = gtk_tree_path_get_indices(path_expand);

					my_indices_num = *idc; // 自己所在的 tree view 的目录级

					// 储存头像, 自己的放在相应根目录下的第一个, 所以要查找上一级的根目录下有多少节点
					// 才好把 face 放到 array_face 下
					if (*idc == 0)
						g_array_insert_val(array_face, 0, pItem->face);
					else
					{
						int n, all_num = 0;
						n = *idc;
						while (n--)
							all_num += g_array_index(array_item_num, int, n);
						g_array_insert_val(array_face, all_num, pItem->face);
					}
					// 以上解决头像在 tree view 和 chat dlg 中的关联问题
					// 因为自己的头像处于所在根目录的第一个位置,
					// 所以确定好其在 array_face 中的位置非常重要

					gtk_tree_store_move_after(store, &child, NULL);

					if (!init_me_first)
					{
						init_me_first = TRUE;

						// 更新头像, 换成自己的头像
						gtk_container_remove(GTK_CONTAINER(button_image),
								image_user);
						image_user_new = gtk_image_new_from_file(gstrface);
						image_user = image_user_new;
						gtk_container_add(GTK_CONTAINER(button_image),
								image_user_new);
						gtk_widget_show_all(button_image);
					}
				}
				else
					// 储存头像
					g_array_append_val(array_face, pItem->face);
			}
		}

	gtk_tree_view_set_model(GTK_TREE_VIEW (treeview), GTK_TREE_MODEL (store));
	if (path_expand != NULL)
		gtk_tree_view_expand_to_path(GTK_TREE_VIEW (treeview), path_expand);
	g_object_unref(store);
}

void row_activated(GtkTreeView *treeview, GtkTreePath *path,
		GtkTreeViewColumn *column, gpointer data)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	model = gtk_tree_view_get_model(treeview);
	if (gtk_tree_model_get_iter(model, &iter, path))
	{
		gboolean haschild;
		// 双击 child 节点弹出聊天对话框
		if (!(haschild = gtk_tree_model_iter_has_child(model, &iter)))
		{
			gchar *item_text, **text_split, *text_id;

			// 获取 parent 和 child 的对应的数字, path_str 值类似 1:2,
			// 表示第 2 个 根节点中的第 3 个孩子节点被双击
			gchar *path_str = gtk_tree_path_to_string(path);
			gchar **path_split = g_strsplit(path_str, ":", 2);
			int parent_num = atoi(path_split[0]);
			int child_num = atoi(path_split[1]);
			int indices_num = parent_num;

			int face_array_index = 0;
			while (parent_num--)
				face_array_index
						+= g_array_index(array_item_num, int, parent_num);
			face_array_index += child_num;

			// 获取真正的 face 数据
			int face_num = g_array_index(array_face, int, face_array_index);

			gtk_tree_model_get(model, &iter, NAME, &item_text, -1);

			text_split = g_strsplit(item_text, "\n", 2);

			// 对话窗口的标题
			text_id = text_split[1];//g_strconcat(text_split[0], text_split[1], NULL);

			if (strcmp(text_id, my_id) == 0)
			{
				show_info_msg_box(GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
						"你不能和自己聊天！");
				return;
			}

			// 不重复建立聊天对话窗口, 已经建立的通过 g_datalist_get_data 获得窗口
			Widgets_Chat
					*w_chatdlg =
							(Widgets_Chat *) g_datalist_get_data(&widget_chat_dlg, text_id);

			if (w_chatdlg == NULL)
			{
				if (strcmp(text_split[0], "Group") == 0) // 群窗口
				{
					// 不允许参与除了自己部门和总群的群聊
					if (my_indices_num != indices_num && indices_num != 0)
					{
						show_info_msg_box(GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
								"您不能和其他组聊天！");
						return;
					}
					else
						w_chatdlg = create_chat_dlg(text_id, face_num, TRUE);
				}
				else
					// 单聊窗口
					w_chatdlg = create_chat_dlg(text_id, face_num, FALSE);

				g_datalist_set_data(&widget_chat_dlg, text_id, w_chatdlg);
			}
			else
				gtk_widget_show_all(w_chatdlg->chat_dlg);

			g_free(item_text);
		}
	}
}

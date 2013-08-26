/*
 * wbkfs: sistema de arquivos de brinquedo para utilização em sala de aula.
 * Versão original escrita por Glauber de Oliveira Costa para a disciplina MC514
 * (Sistemas Operacionais: teoria e prática) no primeiro semestre de 2008.
 * Código atualizado para rodar com kernel 3.10.x.
 *
 * Alteração:   Anderson Coelho Weller
 * Data:        Agosto / 2013
 * Modificação: Inclusão de backup dos arquivos gravados.
 * 
 */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/vfs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mm.h>

#include <asm/current.h>
#include <asm/uaccess.h>


/* O wbkfs eh um fs muito simples. Os inodes sao atribuidos em ordem crescente, sem
 * reaproveitamento */
static int inode_number = 0;

/* Lembre-se que nao temos um disco! (Isso so complicaria as coisas, pois teriamos
 * que lidar com o sub-sistema de I/O. Entao teremos uma representacao bastante
 * simples da estrutura de arquivos: Uma lista duplamente ligada circular (para
 * aplicacoes reais, um hash seria muito mais adequado) contem em cada elemento
 * um indice (inode) e uma pagina para conteudo (ou seja: o tamanho maximo de um
 * arquivo nessa versao do anderson bkfs eh de 4Kb. Nao ha subdiretorios */
struct file_contents {
	struct list_head list;
	struct inode *inode;
	void *conts;
};

/* lista duplamente ligada circular, contendo todos os arquivos do fs */
static LIST_HEAD(contents_list);

static const struct super_operations wbkfs_ops = {
        .statfs         = simple_statfs,
        .drop_inode     = generic_delete_inode,
};

/* Lembram quando eu disse que um hash seria mais eficiente? ;-) */
static struct file_contents *wbkfs_find_file(struct inode *inode)
{
	struct file_contents *f;

printk("*** ANDERSON --- FIND FILE ***\n");

	list_for_each_entry(f, &contents_list, list) {
		if (f->inode == inode)
			return f;
	}
	return NULL;
}

/* Apos passar pelo VFS, uma leitura chegara aqui. A unica
 * coisa que fazemos eh, achar o ponteiro para o conteudo do arquivo,
 * e retornar, de acordo com o tamanho solicitado */
ssize_t wbkfs_read(struct file *file, char __user *buf,
		      size_t count, loff_t *pos)
{
	struct file_contents *f;
printk("*** ANDERSON --- READ (0) ***\n");
	struct inode *inode = file->f_path.dentry->d_inode;
	int size = count;

	f = wbkfs_find_file(inode);
	if (f == NULL)
		return -EIO;
		
	if (f->inode->i_size < count)
		size = f->inode->i_size;

	if ((*pos + size) >= f->inode->i_size)
		size = f->inode->i_size - *pos;
	
	/* As page tables do kernel estao sempre mapeadas (veremos o que
	 * sao page tables mais pra frente do curso), mas o mesmo nao eh
	 * verdade com as paginas de espaco de usuario. Dessa forma, uma
	 * atribuicao de/para um ponteiro contendo um endedereco de espaco
	 * de usuario pode falhar. Dessa forma, toda a comunicacao
	 * de/para espaco de usuario eh feita com as funcoes copy_from_user()
	 * e copy_to_user(). */
	if (copy_to_user(buf, f->conts + *pos, size))
		return -EFAULT;
	*pos += size;
	printk("*** ANDERSON --- READ (F) ***\n");

	return size;
}


/*
 * Protótipos das funções.
 */
static struct inode  *wbkfs_make_inode  (struct super_block *sb, int mode);
static struct dentry *wbkfs_create_file (struct super_block *sb, struct dentry *dir, const char *name);


/* similar a leitura, mas vamos escrever no ponteiro do conteudo.
 * Por simplicidade, estamos escrevendo sempre no comeco do arquivo.
 * Obviamente, esse nao eh o comportamento esperado de um write 'normal'
 * Mas implementacoes de sistemas de arquivos sao flexiveis... */
ssize_t wbkfs_write(struct file *file, const char __user *buf,
		       size_t count, loff_t *pos)
{
	struct file_contents *f;
	struct inode *inode = file->f_path.dentry->d_inode;

	printk("*** ANDERSON --- WRITE ***\n");

	f = wbkfs_find_file(inode);
	if (f == NULL)
		return -ENOENT;


	/* ANDERSON */
	/* Se o nome do arquivo não inicia contém ".BKP", então */
	unsigned char * name = file->f_path.dentry->d_name.name;
	char ext[] = ".BKP";
	if (strstr(name, ext)==NULL) {
		printk(name);
		printk(" *** ANDERSON --- WRITE - Diferente (00 - Iniciando novo Backup) ***\n");

		/* Busca o conteúdo original do arquivo */
		
		/****
		char __user *buf_bkp;
		size_t count_bkp = count;
		loff_t *pos_bkp = pos;
		printk(" *** ANDERSON --- WRITE - (01) ***\n");
		wbkfs_read(file, buf_bkp, count_bkp, pos_bkp);
		printk(" *** ANDERSON --- WRITE - (02) ***\n");
		if (buf_bkp != NULL) {
			printk(buf_bkp);
			printk(" *** ANDERSON --- WRITE - (03a - Obtendo valor original) ***\n");
		} else {
			printk(" *** ANDERSON --- WRITE - (03b - Buffer BKP vazio) ***\n");
		}
		****/
		
		/*****
		char __user *buf_bkp;
		int size = count;
		printk(" *** ANDERSON --- WRITE - (01) ***\n");
		if (f->inode->i_size < count)
			size = f->inode->i_size;
		printk(" *** ANDERSON --- WRITE - (02) ***\n");
		if ((*pos + size) >= f->inode->i_size)
			size = f->inode->i_size - *pos;
		printk(" *** ANDERSON --- WRITE - (03) ***\n");
		if (copy_to_user(buf_bkp, f->conts + *pos, size))
			return -EFAULT;
		printk(" *** ANDERSON --- WRITE - (04) ***\n");
		if (buf_bkp != NULL) {
			printk(buf_bkp);
			printk(" *** ANDERSON --- WRITE - (05a - Obtendo valor original) ***\n");
		} else {
			printk(" *** ANDERSON --- WRITE - (05b - Buffer BKP vazio) ***\n");
		}
		******/
		
		/* Cria um novo arquivo (Terminando com ".BKP" */
		char novo[80] = "";
		strcat(novo, name);
		strcat(novo, ext);
		printk(novo);
		
		printk(" *** ANDERSON --- WRITE - (Novo arquivo) ***\n");
		
		// Criar um novo arquivo (dentry/inode - com o novo nome)
		wbkfs_create_file(inode->i_sb, inode->i_sb->s_root, novo);
		printk(" *** ANDERSON --- WRITE - (Arquivo criado) ***\n");
		
		/* Grava o conteúdo desse arquivo */
		/*
		 * 
		 */
		
	} else {
		printk(name);
		printk(" *** ANDERSON --- WRITE - Igual (NAO feito novo Backup) ***\n");
	}


	/* copy_from_user() : veja comentario na funcao de leitura */
	if (copy_from_user(f->conts + *pos, buf, count))
		return -EFAULT;

	inode->i_size = count;

	return count;
}

static int wbkfs_open(struct inode *inode, struct file *file)
{
	/* Todo arquivo tem uma estrutura privada associada a ele.
	 * Em geral, estamos apenas copiando a do inode, se houver. Mas isso
	 * eh tao flexivel quanto se queira, e podemos armazenar aqui
	 * qualquer tipo de coisa que seja por-arquivo. Por exemplo: Poderiamos
	 * associar um arquivo /mydir/4321 com o processo no 4321 e guardar aqui
	 * a estrutura que descreve este processo */

printk("*** ANDERSON --- OPEN ***\n");

	if (inode->i_private)
		file->private_data = inode->i_private;
	return 0;
}


const struct file_operations wbkfs_file_operations = {
	.read  = wbkfs_read,
	.write = wbkfs_write,
	.open  = wbkfs_open,
};

static const struct inode_operations wbkfs_file_inode_operations = {
	.getattr        = simple_getattr,
};


/*
 * Anytime we make a file or directory in our filesystem we need to
 * come up with an inode to represent it internally.  This is
 * the function that does that job.  All that's really interesting
 * is the "mode" parameter, which says whether this is a directory
 * or file, and gives the permissions. (Adaptado de LWNFS).
 */
static struct inode *wbkfs_make_inode(struct super_block *sb, int mode)
{
	struct inode *inode = new_inode(sb);

	if (inode) {
		inode->i_mode   = mode | S_IFREG;
		inode->i_uid    = current->cred->fsuid;
		inode->i_gid    = current->cred->fsgid;
		inode->i_blocks = 0;
		inode->i_atime  = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
		inode->i_ino    = ++inode_number;
		inode->i_op     = &wbkfs_file_inode_operations;
		inode->i_fop    = &wbkfs_file_operations;
	}
	return inode;
}


/* criacao de um arquivo: sem misterio, sem segredo, apenas
 * alocar as estruturas, preencher, e retornar */
static int wbkfs_create (struct inode *dir, struct dentry * dentry,
			    umode_t mode, bool excl)
{
	struct inode *inode;
	struct file_contents *file = kmalloc(sizeof(*file), GFP_KERNEL);	
	struct page *page;

	printk("*** ANDERSON --- CREATE ***\n");
	unsigned char * name = dentry->d_name.name;
	printk(name);
	printk("*** ANDERSON --- Fim printk dentry name.\n");

	if (!file)
		return -EAGAIN;

	/* Cria um novo inode */
	inode = wbkfs_make_inode(dir->i_sb, mode);
	
	/********************************
	inode = new_inode(dir->i_sb);
	inode->i_mode = mode | S_IFREG;
	inode->i_uid = current->cred->fsuid;
	inode->i_gid = current->cred->fsgid;
	inode->i_blocks = 0;
	inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
        inode->i_ino = ++inode_number;
	inode->i_op  = &wbkfs_file_inode_operations;
	inode->i_fop = &wbkfs_file_operations;
	********************************/

	file->inode = inode;
	page = alloc_page(GFP_KERNEL);
	if (!page)
		goto cleanup;

	file->conts = page_address(page);
	INIT_LIST_HEAD(&file->list);
	list_add_tail(&file->list, &contents_list); 
	d_instantiate(dentry, inode);  
	dget(dentry);

	return 0;
cleanup:
	iput(inode);
	kfree(file);
	return -EINVAL;
}


/*
 * Create a file mapping a name to a counter (Adaptado de LWNFS).
 */
static struct dentry *wbkfs_create_file (struct super_block *sb,
		struct dentry *dir, const char *name)
{
	struct dentry *dentry;
	struct inode  *inode;
	struct qstr   qname;
	/*
	 * Make a hashed version of the name to go with the dentry.
	 */
	qname.name = name;
	qname.len  = strlen (name);
	qname.hash = full_name_hash(name, qname.len);
	
	printk("*** ANDERSON --- CREATE_FILE ***\n");
	printk(name);
	printk(" *** ANDERSON --- qname.name.\n");

	/*
	 * Now we can create our dentry and the inode to go with it.
	 */
	dentry = d_alloc(dir, &qname);
	if (! dentry)
		goto out;
	/*
	 * Cria o inode
	 */	
	///wbkfs_create(dir, dentry, 0644, false);
	inode = wbkfs_make_inode(sb, 0644); //S_IFREG | 
	if (! inode)
		goto out_dput;
	inode->i_fop     = &wbkfs_ops;
	inode->i_private = 0;
	/*
	 * Put it all into the dentry cache and we're done.
	 */
	d_add(dentry, inode);
	return dentry;
/*
 * Then again, maybe it didn't work.
 */
  out_dput:
	dput(dentry);
  out:
	return 0;
}




static const struct inode_operations wbkfs_dir_inode_operations = {
        .create         = wbkfs_create,
	.lookup		= simple_lookup,
};


static int wbkfs_fill_super(struct super_block *sb, void *data, int silent)
{
        struct inode * inode;
        struct dentry * root;

printk("*** ANDERSON --- FILL SUPER ***\n");

        sb->s_maxbytes = 4096;
        sb->s_magic = 0xBEBACAFE;
	sb->s_blocksize = 1024;
	sb->s_blocksize_bits = 10;

        sb->s_op = &wbkfs_ops;
        sb->s_time_gran = 1;

        inode = new_inode(sb);

        if (!inode)
                return -ENOMEM;

        inode->i_ino = ++inode_number;
        inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
        inode->i_blocks = 0;
        inode->i_uid = inode->i_gid = 0;
        inode->i_mode = S_IFDIR | S_IRUGO | S_IXUGO | S_IWUSR;
        inode->i_op = &wbkfs_dir_inode_operations;
        inode->i_fop = &simple_dir_operations;
        set_nlink(inode, 2);

        root = d_make_root(inode);
        if (!root) {
                iput(inode);
                return -ENOMEM;
        }
        sb->s_root = root;
        return 0;

}

static struct dentry *wbkfs_get_sb(struct file_system_type *fs_type, int flags, const char *dev_name,
		   void *data)
{
printk("*** ANDERSON --- GET_SB ***\n");

  return mount_bdev(fs_type, flags, dev_name, data, wbkfs_fill_super);
}

static struct file_system_type wbkfs_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "wbkfs",
	.mount		= wbkfs_get_sb,
	.kill_sb	= kill_litter_super,
};

static int __init init_wbkfs_fs(void)
{
printk("*** ANDERSON --- INIT ***\n");

	INIT_LIST_HEAD(&contents_list);
	return register_filesystem(&wbkfs_fs_type);
}

static void __exit exit_wbkfs_fs(void)
{
	unregister_filesystem(&wbkfs_fs_type);
}

module_init(init_wbkfs_fs)
module_exit(exit_wbkfs_fs)
MODULE_LICENSE("GPL");

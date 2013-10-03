WBkFS
=========================

Weller BacKup File System

=========================

Autor: Anderson Coelho Weller

1) Sistema de arquivos com backup integrado (WBkFS)

	WBkFS: Sistema de arquivos de brinquedo, com controle de subdiretórios, que realiza backup dos arquivos gravados.
	
	Funcionalidade: Realização de backup dos arquivos, antes deles serem efetivamente alterados.

	Ao gravar algum arquivo, o "WBkFS" armazena o conteúdo original em um arquivo oculto de backup (iniciando com ponto [.] e com extensão .BKP). (Ex.: Grava em ".anderson.txt.BKP" o conteúdo que estava em "anderson.txt" antes deste ser alterado).


2) Origem do "WBkFS":

	- Surgiu a partir de alterações no "IsleneFS" em conjunto com o "LwnFS" e o GOGIsleneFS, como parte dos trabalhos realizados na matéria "MO806 - Tópicos em Sistemas Operacionais" do mestrado em computação da Unicamp, no 2º semestre de 2013.

3) Modificações nos diretórios/arquivos de fonte do kernel, para a inclusão do novo sistema de arquivos:

	- Mudando para o diretório inicial do código fonte do kernel, por exemplo:
		cd /home/anderson/linux-3.10.6/

	- Criação de diretório:
		mkdir -p ./fs/wbkfs

	- Criação do Makefile (nesse diretório) com o conteúdo (obj-y += wbkfs.o):
		echo "obj-y += wbkfs.o" > ./fs/wbkfs/Makefile

	- Alteração do Makefile do diretório ../fs, incluindo linha para o novo diretório (obj-y += wbkfs/):
		cp ./fs/Makefile ./fs/Makefile.old
		echo "obj-y += wbkfs/" >> ./fs/Makefile

4) Compilação do Kernel:

	- Continuando no diretório inicial do código fonte do kernel, por exemplo:
		cd /home/anderson/linux-3.10.6/

	- Compilar o kernel do linux (Calculando o tempo (time) e gerando arquivos ".deb" para instalação do kernel com o "dpkg -i ...")
		time fakeroot make-kpkg -j1 --initrd --revision=3.10.6 --append-to-version=wbkfs.01 kernel_image kernel_headers

5) Testar o funcionamento do novo FileSystem

	- Abrir o QEMU com o novo Kernel (Exemplo utilizando a imagem "../mo806.img")
		qemu-system-i386 -hda ../mo806.img -kernel arch/i386/boot/bzImage -append "ro root=/dev/hda"

	- Criar um novo disco virtual (para utilizar com o WBkFS)
		dd if=/dev/zero of=virtual.dsk bs=1k count=40

	- Montar o disco virtual
		mkdir mnt
		mount -t wbkfs -o loop virtual.dsk mnt/

	- Acessar o diretório montado
		cd mnt

	- Realizar os testes:
		cat > a.txt
		[... digitar texto ... Pressionar ENTER ...]
		[... Pressionar CTRL+D para encerrar o "cat" ...]

	- Verificar o resultado dos testes (digitar os comandos e verificar os resultados):
		ls -la
		cat a.txt
		cat a.txt.BKP

	- Desmontar a unidade de testes:
		cd ..
		umount mnt

	- Fechar o QEMU
		shutdown -h now

6) Detalhes da implementação:

	- inode->i_private - Armazena o conteúdo do arquivo.

	- inode->i_size    - Armazena o número de bytes gravados no arquivo.

7) Principais Métodos:

	// Localizar o arquivo de backup (que deve estar no mesmo diretório do arquivo original).
	struct inode *wbkfs_find_bkp(struct file *file, const char *name)

	// Grava o conteúdo de "buf" no respectivo inode, e grava o conteúdo antigo no inode de backup.
	ssize_t wbkfs_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)

	// Cria de um arquivo (Chamado pelo sistema)
	static int wbkfs_create (struct inode *dir, struct dentry * dentry, umode_t mode, bool excl)

	// Criação do arquivo de backup
	static struct inode *wbkfs_create_backup(struct super_block *sb, struct dentry *dir, const char *name)

	// Cria um novo inode.
	static struct inode *wbkfs_make_inode(struct super_block *sb, int mode)


8) Problemas conhecidos:

	- Por algum motivo o valor de "inode->i_size" aparece zerado no método Write. Por esse motivo, é utilizado o método "strlen" para encontrar esse valor.

	- Em alguns casos, aparecem lixo apos o conteúdo gravado no inode.


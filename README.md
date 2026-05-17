# Modul 4: Filesystem & Docker (WIP)
---
Nama: Pradipta Airlanggga Ramadhan

NRP: 5027251118

Departemen: Teknologi Informasi

---

## Tree
![alt text](image.png)
---

## Soal 1 - Save Asisten Kenz
Author: Asisten_Kalkuluz/Ibnu

### Penjelasan Soal
Pada soal ini kita diminta membuat program FUSE (Filesystem in Userspace) dalam bahasa C bernama kenz_rescue.c. Program harus melakukan dua hal utama. Pertama, passthrough, setiap file 1.txt sampai 7.txt yang ada di amba_files/ harus terlihat di mnt/ dengan isi yang byte-identik sama persis. Kedua, file virtual menambahkan satu file bernama tujuan.txt yang hanya muncul di mnt/ tetapi tidak ada secara fisik di amba_files/. Isi tujuan.txt dibangkitkan on-the-fly saat dibaca, yaitu dengan menggabungkan semua fragmen yang diawali KOORD: dari ketujuh file secara urut, lalu diformat menjadi Tujuan Mas Amba: <gabungan_fragmen>.
### Penjelasan Kode
#### A. Header dan Global Variable
```c
#define FUSE_USE_VERSION 28
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
// source_directory adalah path ke amba_files/
static const char *source_dir;
```
Versi API FUSE yang digunakan yaitu versi 2.8, lalu menyertakan semua header yang dibutuhkan. fuse.h adalah header utama library FUSE. Header lainnya seperti dirent.h, sys/stat.h, dan fcntl.h dibutuhkan untuk operasi file dan direktori. Variabel global source_dir menyimpan path ke folder amba_files/ agar bisa diakses oleh semua fungsi callback tanpa perlu dioper sebagai parameter.

#### B. get_full_path()
```c
static void get_full_path(char *full_path, const char *path) {
    snprintf(full_path, 1024, "%s%s", source_dir, path);
}
```
Fungsi bantu ini menggabungkan source_dir dengan path relatif yang diterima FUSE menjadi path lengkap ke file asli di amba_files/. Misalnya jika source_dir = "/home/user/.../amba_files" dan path = "/1.txt", maka hasilnya adalah /home/user/.../amba_files/1.txt. Fungsi ini dipanggil oleh kenz_getattr, kenz_open, dan kenz_read setiap kali perlu mengakses file asli.

#### C. build_tujuan_content()
```c
static char *build_tujuan_content() {
    static char content[4096];
    char result[2048];
    result[0] = '\0';

    // Loop file 1.txt sampai 7.txt secara urut
    for (int i = 1; i <= 7; i++) {
        char filepath[256];
        snprintf(filepath, sizeof(filepath), "%s/%d.txt", source_dir, i);

        FILE *f = fopen(filepath, "r");
        if (!f) continue;

        char line[512];
        while (fgets(line, sizeof(line), f)) {
            // Cari baris yang diawali "KOORD:"
            if (strncmp(line, "KOORD:", 6) == 0) {
                // Ambil bagian setelah "KOORD: "
                char *fragment = line + 6;
                // Hilangkan spasi di depan
                while (*fragment == ' ') fragment++;
                // Hilangkan newline di belakang
                int len = strlen(fragment);
                while (len > 0 && (fragment[len-1] == '\n' || fragment[len-1] == '\r')) {
                    fragment[len-1] = '\0';
                    len--;
                }
                // Tambahkan ke result (gabungkan semua fragmen)
                strcat(result, fragment);
            }
        }
        fclose(f);
    }

    // Format output: "Tujuan Mas Amba: <gabungan_fragmen>\n"
    snprintf(content, sizeof(content), "Tujuan Mas Amba: %s\n", result);
    return content;
}
```
Fungsi ini membaca file 1.txt sampai 7.txt secara urut dan mencari baris yang diawali KOORD: menggunakan strncmp. Setiap fragmen yang ditemukan dibersihkan dari spasi di depan dan karakter newline di belakang, lalu digabungkan satu per satu ke string result. Setelah semua file selesai dibaca, hasilnya diformat menjadi "Tujuan Mas Amba: <result>\n" dan dikembalikan. Variabel content dibuat static agar isinya tetap valid setelah fungsi selesai. Fungsi ini dipanggil on-the-fly setiap kali tujuan.txt dibaca, bukan disimpan di disk.

#### D. kenz_getattr()
```c
static int kenz_getattr(const char *path, struct stat *stbuf) {
    memset(stbuf, 0, sizeof(struct stat));

    // Jika path adalah root "/"
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }

    // Jika path adalah file virtual "/tujuan.txt"
    if (strcmp(path, "/tujuan.txt") == 0) {
        stbuf->st_mode = S_IFREG | 0444;  // read-only
        stbuf->st_nlink = 1;
        // Hitung ukuran konten tujuan.txt secara dinamis
        char *content = build_tujuan_content();
        stbuf->st_size = strlen(content);
        return 0;
    }

    // Selain itu, passthrough ke source_dir
    char full_path[1024];
    get_full_path(full_path, path);

    int res = lstat(full_path, stbuf);
    if (res == -1) return -errno;

    return 0;
}
```
Fungsi ini adalah callback FUSE yang dipanggil setiap kali sistem membutuhkan metadata sebuah file atau folder, misalnya saat menjalankan ls atau stat. Ada tiga kasus yang ditangani: jika path adalah / maka diisi sebagai direktori dengan permission 0755; jika path adalah /tujuan.txt maka diisi sebagai file reguler read-only (0444) dengan ukuran yang dihitung dari panjang konten yang dibangkitkan; selain itu dilakukan passthrough yaitu mengambil metadata langsung dari file asli di amba_files/ menggunakan lstat.

#### E. kenz_readdir()
```c
static int kenz_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                        off_t offset, struct fuse_file_info *fi) {
    (void) offset;
    (void) fi;

    if (strcmp(path, "/") != 0)
        return -ENOENT;

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    DIR *dp = opendir(source_dir);
    if (!dp) return -errno;

    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;
        filler(buf, de->d_name, NULL, 0);
    }
    closedir(dp);
    filler(buf, "tujuan.txt", NULL, 0);

    return 0;
}
```
Fungsi ini dipanggil saat user menjalankan ls mnt/. Cara kerjanya adalah membuka amba_files/ dengan opendir, lalu membaca setiap entry menggunakan readdir dan memasukkannya ke listing mount dengan filler. Entry . dan .. di-skip agar tidak duplikat karena sudah ditambahkan manual di awal. Setelah semua file asli dimasukkan, tujuan.txt ditambahkan secara manual sebagai file virtual. Hasilnya ls mnt/ menampilkan delapan entry sedangkan ls amba_files/ hanya tujuh.

#### F. kenz_open()
```c
static int kenz_open(const char *path, struct fuse_file_info *fi) {
    if (strcmp(path, "/tujuan.txt") == 0) {
        if ((fi->flags & O_ACCMODE) != O_RDONLY)
            return -EACCES;
        return 0;
    }

    char full_path[1024];
    get_full_path(full_path, path);

    int res = open(full_path, fi->flags);
    if (res == -1) return -errno;

    close(res);
    return 0;
}
```
Fungsi ini dipanggil saat sebuah file hendak dibuka. Untuk file virtual tujuan.txt, fungsi memeriksa flag akses — jika bukan read-only maka dikembalikan error EACCES karena file virtual tidak bisa ditulis. Untuk file lainnya (1.txt–7.txt), dilakukan passthrough yaitu mencoba membuka file asli di amba_files/ untuk memvalidasi bahwa file tersebut memang bisa diakses, lalu langsung ditutup kembali.

#### G. kenz_read()
```c
static int kenz_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) { 
    (void) fi;

    if (strcmp(path, "/tujuan.txt") == 0) {
        char *content = build_tujuan_content();
        size_t len = strlen(content);

        if (offset >= (off_t)len) return 0;
        if (offset + size > len) size = len - offset;

        memcpy(buf, content + offset, size);
        return size;
    }

    char full_path[1024];
    get_full_path(full_path, path);

    int fd = open(full_path, O_RDONLY);
    if (fd == -1) return -errno;

    int res = pread(fd, buf, size, offset);
    if (res == -1) res = -errno;

    close(fd);
    return res;
}
```
ungsi ini dipanggil saat isi file dibaca, misalnya oleh perintah cat. Untuk tujuan.txt, konten dibangkitkan on-the-fly dengan memanggil build_tujuan_content(), lalu disalin ke buffer output sesuai offset dan ukuran yang diminta. Untuk file lainnya, dilakukan passthrough dengan membuka file asli menggunakan open lalu membacanya dengan pread yang mendukung pembacaan dari posisi offset tertentu.

#### H. fuse_operation dan main()
```c
static struct fuse_operations kenz_oper = {
    .getattr = kenz_getattr,
    .readdir = kenz_readdir,
    .open    = kenz_open,
    .read    = kenz_read,
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source_directory> <mount_directory>\n", argv[0]);
        return 1;
    }
    source_dir = argv[1];
    argv[1] = argv[2];
    argc--;
    return fuse_main(argc, argv, &kenz_oper, NULL);
}
```
Struct fuse_operations mendaftarkan semua fungsi callback yang akan dipanggil FUSE sesuai operasi yang dilakukan user. Di main, argumen pertama (argv[1]) disimpan ke source_dir sebagai path ke amba_files/, lalu argumen digeser sehingga argv[1] menjadi mount directory karena fuse_main hanya butuh satu argumen yaitu mount point. Setelah itu fuse_main dipanggil untuk menyerahkan kendali sepenuhnya ke library FUSE.
### Compiler dan Jalankan
```
# Download dan unzip amba_files
unzip amba_files.zip
rm amba_files.zip

# Buat folder mount
mkdir -p mnt
```
#### Compile
```
gcc -Wall -o kenz_rescue kenz_rescue.c $(pkg-config --cflags --libs fuse)
```

#### Jalan
```
./kenz_rescue $(pwd)/amba_files $(pwd)/mnt
```
#### Buka terminal baru
```
# Cek mountpoint
mountpoint mnt

# Lihat isi mnt
ls mnt/

# Bandingkan dengan amba_files
ls amba_files/

# Test passthrough byte-by-byte
for i in 1 2 3 4 5 6 7; do
    diff mnt/$i.txt amba_files/$i.txt && echo "$i.txt OK"
done

# Baca file virtual
cat mnt/tujuan.txt

# Pastikan tujuan.txt tidak ada di amba_files
ls amba_files/tujuan.txt 2>&1

# Cek metadata tujuan.txt
stat mnt/tujuan.txt
```

#### Unmount
```
fusermount -u mnt
```

### Input dan Output
<img width="945" height="37" alt="image" src="https://github.com/user-attachments/assets/302ca950-a707-4686-bd00-5a0f05814381" />

<img width="667" height="450" alt="image" src="https://github.com/user-attachments/assets/a991be34-0405-4f38-afaf-062b26cf93d3" />

### Bug dan kesulitan saat mengerjakan
Selama pengerjaan ditemukan beberapa bug yaitu typo `f(!f)` seharusnya `if(!f)`, `strcmp` seharusnya `strncmp`, nama variabel `fragment` hilang di kondisi while, `char* full_path` seharusnya `char full_path`, nama fungsi typo double `n` menyebabkan undefined reference, nama file virtual tidak konsisten antara `/Tujuan.txt` dan `/tujuan.txt`, serta path `source_dir` harus absolut menggunakan `$(pwd)`.

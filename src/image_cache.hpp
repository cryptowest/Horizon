#ifndef IMAGE_CACHE_HPP
#define IMAGE_CACHE_HPP
#include <vector>
#include <set>
#include <deque>
#include <memory>
#include <glibmm/variant.h>
#include <glibmm/threads.h>
#include <giomm/file.h>
#include <giomm/memoryinputstream.h>
#include <gdkmm/pixbufloader.h>
#include "thread.hpp"
#include "canceller.hpp"

#ifdef HAVE_EV___H
#include <ev++.h>
#else
#include <libev/ev++.h>
#endif


namespace Horizon {
	struct VariantTypeDeleter {
		void operator()(GVariantType* vt) {
			g_variant_type_free(vt);
		}
	};

	struct VariantUnrefer {
		void operator()(GVariant* v) {
			g_variant_unref(v);
		}
	};

	class ImageData {
	public:
		ImageData() = delete;
		ImageData(const ImageData&) = delete;
		ImageData& operator=(const ImageData&) = delete;
		~ImageData() = default;
		ImageData(const guint32 version, const Glib::VariantContainerBase &);
		explicit ImageData(const guint32 version, std::unique_ptr<GVariant, VariantUnrefer> cvariant);
		ImageData(const Glib::RefPtr<Post> &post);

	public:
		void update(const Glib::RefPtr<Post> &post);
		void merge(const std::unique_ptr<ImageData>&);

		Glib::VariantContainerBase get_variant() const;
		GVariant* get_cvariant() const;
		Glib::ustring get_uri(bool thumb) const;

		guint64 size;
		std::string md5;
		std::string ext;
		std::set<Glib::ustring> boards;
		std::set<Glib::ustring> tags;
		std::set<std::string> original_filenames; // Potentially UNIX + milli
		std::set<std::string> poster_names;
		std::set<gint64> posted_unix_dates;
		guint16 num_spoiler;
		guint16 num_deleted;
		bool have_thumbnail;
		bool have_image;
	};

	class ImageCache {
	public:
		ImageCache(const Glib::RefPtr<Gio::File>& cache_file);
		~ImageCache();

		void merge_file(const Glib::RefPtr<Gio::File>& merge_file);

		bool has_thumb(const Glib::RefPtr<Post> &post);
		bool has_image(const Glib::RefPtr<Post> &post);

		void get_thumb_async(const Glib::RefPtr<Post> &post,
		                     std::function<void (const Glib::RefPtr<Gdk::PixbufLoader>&)> callback,
		                     std::shared_ptr<Canceller> canceller );
		void get_image_async(const Glib::RefPtr<Post> &post,
		                     std::function<void (const Glib::RefPtr<Gdk::PixbufLoader>&)> callback,
		                     std::shared_ptr<Canceller> canceller );

		void write_thumb_async(const Glib::RefPtr<Post> &post,
		                       Glib::RefPtr<Gio::MemoryInputStream> &istream);
		void write_image_async(const Glib::RefPtr<Post> &post,
		                       Glib::RefPtr<Gio::MemoryInputStream> &istream);

		void clean_invalid();
		void flush();

	private:
		Glib::RefPtr<Gio::File> cache_file;

		mutable Glib::Threads::Mutex map_lock;
		std::map<std::string, std::unique_ptr<ImageData> > images;

		mutable Glib::Threads::Mutex thumb_write_queue_lock;
		std::deque< std::pair< Glib::RefPtr<Post>,
		                       Glib::RefPtr<Gio::MemoryInputStream> > > thumb_write_queue;
		void write(const Glib::RefPtr<Post> &,
		           Glib::RefPtr<Gio::MemoryInputStream>,
		           const bool write_thumb);
		                 
		mutable Glib::Threads::Mutex image_write_queue_lock;
		std::deque< std::pair< Glib::RefPtr<Post>,
		                       Glib::RefPtr<Gio::MemoryInputStream> > > image_write_queue;

		mutable Glib::Threads::Mutex thumb_read_queue_lock;
		std::deque< std::tuple< Glib::RefPtr<Post>,
		                        std::function<void (const Glib::RefPtr<Gdk::PixbufLoader>&)>,
		                        std::shared_ptr<Canceller> > > thumb_read_queue;
		void read_thumb(const Glib::RefPtr<Post>&,
		                std::function<void (const Glib::RefPtr<Gdk::PixbufLoader>&)>,
		                std::shared_ptr<Canceller> canceller);

		mutable Glib::Threads::Mutex image_read_queue_lock;
		std::deque< std::tuple< Glib::RefPtr<Post>,
		                        std::function<void (const Glib::RefPtr<Gdk::PixbufLoader>&)>,
		                        std::shared_ptr<Canceller> > > image_read_queue;
		void read_image(const Glib::RefPtr<Post> &,
		                std::function<void (const Glib::RefPtr<Gdk::PixbufLoader>&)>,
		                std::shared_ptr<Canceller> canceller);

		void read(const Glib::RefPtr<Gio::File>&,
		          std::function<void (const Glib::RefPtr<Gdk::PixbufLoader>&)>,
		          std::shared_ptr<Canceller> canceller);

		void             read_from_disk(const Glib::RefPtr<Gio::File>& cache_file,
		                                const bool make_dirs);

		Glib::Threads::Thread *ev_thread;
		ev::dynamic_loop ev_loop;
		void             loop();

		ev::async        kill_loop_w;
		void             on_kill_loop_w(ev::async &w, int);

		ev::async        write_queue_w;
		void             on_write_queue_w(ev::async &w, int);

		ev::async        read_queue_w;
		void             on_read_queue_w(ev::async &w, int);

		ev::async        flush_w;
		void             on_flush_w(ev::async &, int);

		ev::idle         idle_w;
		void             on_idle_w(ev::idle &, int);

		ev::timer        timer_w;
		void             on_timer_w(ev::timer &, int);

	};

	constexpr char CACHE_FILENAME[] = "horizon-cache.dat";
	constexpr char CACHE_MERGE_FILENAME[] = "horizon-cache.merge";
	constexpr guint32 CACHE_FILE_VERSION = 1;
	constexpr char CACHE_VERSION_1_TYPE[] = "(tayayasasaayaayaxqqbb)";
	constexpr char CACHE_VERSION_1_ARRAYTYPE[] = "a(tayayasasaayaayaxqqbb)";
}


#endif

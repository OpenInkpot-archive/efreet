/* vim: set sw=4 ts=4 sts=4 et: */
#include "Efreet.h"
#include "efreet_private.h"

static char *efreet_icon_deprecated_user_dir = NULL;
static char *efreet_icon_user_dir = NULL;
static Ecore_Hash *efreet_icon_dirs_cached = NULL;
static Ecore_Hash *efreet_icon_themes = NULL;
Ecore_List *efreet_icon_extensions = NULL;
static Ecore_List *efreet_extra_icon_dirs = NULL;
static Ecore_List *efreet_icon_cache = NULL;

static int efreet_icon_init_count = 0;

typedef struct Efreet_Icon_Cache Efreet_Icon_Cache;
struct Efreet_Icon_Cache
{
    char *key;
    char *path;
    time_t lasttime;
};

static char *efreet_icon_remove_extension(const char *icon);
static Efreet_Icon_Theme *efreet_icon_find_theme_check(const char *theme_name);


static char *efreet_icon_find_fallback(Efreet_Icon_Theme *theme,
                                       const char *icon,
                                       const char *size);
static char *efreet_icon_list_find_fallback(Efreet_Icon_Theme *theme,
                                            Ecore_List *icons,
                                            const char *size);
static char *efreet_icon_find_helper(Efreet_Icon_Theme *theme,
                                     const char *icon, const char *size);
static char *efreet_icon_list_find_helper(Efreet_Icon_Theme *theme,
                                          Ecore_List *icons, const char *size);
static char *efreet_icon_lookup_icon(Efreet_Icon_Theme *theme,
                                     const char *icon_name, const char *size);
static char *efreet_icon_fallback_icon(const char *icon_name);
static char *efreet_icon_fallback_dir_scan(const char *dir,
                                           const char *icon_name);

static char *efreet_icon_lookup_directory(Efreet_Icon_Theme *theme,
                                          Efreet_Icon_Theme_Directory *dir,
                                          const char *icon_name);
static int efreet_icon_directory_size_distance(Efreet_Icon_Theme_Directory *dir,
                                                    unsigned int size);
static int efreet_icon_directory_size_match(Efreet_Icon_Theme_Directory *dir,
                                                  unsigned int size);

static Efreet_Icon *efreet_icon_new(const char *path);
static void efreet_icon_point_free(Efreet_Icon_Point *point);
static void efreet_icon_populate(Efreet_Icon *icon, const char *file);

static char *efreet_icon_lookup_directory_helper(Efreet_Icon_Theme_Directory *dir,
                                    const char *path, const char *icon_name);

static Efreet_Icon_Theme *efreet_icon_theme_new(void);
static void efreet_icon_theme_free(Efreet_Icon_Theme *theme);
static void efreet_icon_theme_dir_scan_all(const char *theme_name);
static void efreet_icon_theme_dir_scan(const char *dir,
                                        const char *theme_name);
static void efreet_icon_theme_dir_validity_check(void);
static void efreet_icon_theme_path_add(Efreet_Icon_Theme *theme,
                                                const char *path);
static void efreet_icon_theme_index_read(Efreet_Icon_Theme *theme,
                                                const char *path);

static Efreet_Icon_Theme_Directory *efreet_icon_theme_directory_new(Efreet_Ini *ini,
                                                                const char *name);
static void efreet_icon_theme_directory_free(Efreet_Icon_Theme_Directory *dir);

static void efreet_icon_theme_cache_check(Efreet_Icon_Theme *theme);
static int efreet_icon_theme_cache_check_dir(Efreet_Icon_Theme *theme,
                                                        const char *dir);

static int efreet_icon_cache_find(Efreet_Icon_Cache *value, const char *key);
static void efreet_icon_cache_flush(void);
static void efreet_icon_cache_free(Efreet_Icon_Cache *value);

/**
 * @internal
 * @return Returns 1 on success or 0 on failure
 * @brief Initializes the icon system
 */
int
efreet_icon_init(void)
{
    if (efreet_icon_init_count++ > 0)
        return efreet_icon_init_count;

    if (!efreet_icon_themes)
    {
        const char *default_exts[] = {".png", ".xpm", NULL};
        int i;

        if (!ecore_init())
        {
            efreet_icon_init_count--;
            return 0;
        }

        /* setup the default extension list */
        efreet_icon_extensions = ecore_list_new();
        ecore_list_free_cb_set(efreet_icon_extensions, free);

        for (i = 0; default_exts[i] != NULL; i++)
            ecore_list_append(efreet_icon_extensions, strdup(default_exts[i]));

        efreet_icon_themes = ecore_hash_new(NULL, NULL);
        ecore_hash_free_value_cb_set(efreet_icon_themes,
                            ECORE_FREE_CB(efreet_icon_theme_free));
        efreet_extra_icon_dirs = ecore_list_new();

        efreet_icon_cache = ecore_list_new();
        ecore_list_free_cb_set(efreet_icon_cache, ECORE_FREE_CB(efreet_icon_cache_free));
    }

    return 1;
}

/**
 * @internal
 * @return Returns no value
 * @brief Shuts down the icon system
 */
void
efreet_icon_shutdown(void)
{
    if (--efreet_icon_init_count)
        return;

    IF_FREE(efreet_icon_user_dir);
    IF_FREE(efreet_icon_deprecated_user_dir);

    IF_FREE_LIST(efreet_icon_extensions);
    IF_FREE_HASH(efreet_icon_themes);
    IF_FREE_HASH(efreet_icon_dirs_cached);
    IF_FREE_LIST(efreet_extra_icon_dirs);

    IF_FREE_LIST(efreet_icon_cache);

    ecore_shutdown();
    efreet_icon_init_count = 0;
}

/**
 * @return Returns the user icon directory
 * @brief Returns the user icon directory
 */
const char *
efreet_icon_deprecated_user_dir_get(void)
{
    const char *user;
    int len;

    if (efreet_icon_deprecated_user_dir) return efreet_icon_deprecated_user_dir;

    user = efreet_home_dir_get();
    len = strlen(user) + strlen("/.icons") + 1;
    efreet_icon_deprecated_user_dir = malloc(sizeof(char) * len);
    snprintf(efreet_icon_deprecated_user_dir, len, "%s/.icons", user);

    return efreet_icon_deprecated_user_dir;
}

const char *
efreet_icon_user_dir_get(void)
{
    const char *user;
    int len;

    if (efreet_icon_user_dir) return efreet_icon_user_dir;

    user = efreet_data_home_get();
    len = strlen(user) + strlen("/icons") + 1;
    efreet_icon_user_dir = malloc(sizeof(char) * len);
    snprintf(efreet_icon_user_dir, len, "%s/icons", user);

    return efreet_icon_user_dir;
}

/**
 * @param ext: The extension to add to the list of checked extensions
 * @return Returns no value.
 * @brief Adds the given extension to the list of possible icon extensions
 */
void
efreet_icon_extension_add(const char *ext)
{
    ecore_list_prepend(efreet_icon_extensions, strdup(ext));
}

/**
 * @return Returns a list of strings that are paths to other icon directories
 * @brief Gets the list of all extra directories to look for icons. These
 * directories are used to look for icons after looking in the user icon dir
 * and before looking in standard system directories. The order of search is
 * from first to last directory in this list. the strings in the list should
 * be created with ecore_string_instance().
 */
Ecore_List *
efreet_icon_extra_list_get(void)
{
    return efreet_extra_icon_dirs;
}

/**
 * @return Returns a list of Efreet_Icon structs for all the non-hidden icon
 * themes
 * @brief Retrieves all of the non-hidden icon themes available on the system.
 * The returned list must be freed. Do not free the list data.
 */
Ecore_List *
efreet_icon_theme_list_get(void)
{
    Ecore_List *list, *theme_list;
    char *dir;

    /* update the list to include all icon themes */
    efreet_icon_theme_dir_scan_all(NULL);
    efreet_icon_theme_dir_validity_check();

    /* create the list for the user */
    list = ecore_list_new();
    theme_list = ecore_hash_keys(efreet_icon_themes);
    ecore_list_first_goto(theme_list);
    while ((dir = ecore_list_next(theme_list)))
    {
        Efreet_Icon_Theme *theme;

        theme = ecore_hash_get(efreet_icon_themes, dir);
        if (theme->hidden || theme->fake) continue;
#ifndef STRICT_SPEC
        if (!theme->name.name) continue;
#endif

        ecore_list_append(list, theme);
    }
    ecore_list_destroy(theme_list);

    return list;
}

/**
 * @param theme_name: The theme to look for
 * @return Returns the icon theme related to the given theme name or NULL if
 * none exists.
 * @brief Tries to get the icon theme structure for the given theme name
 */
Efreet_Icon_Theme *
efreet_icon_theme_find(const char *theme_name)
{
    const char *key;
    Efreet_Icon_Theme *theme;

    key = ecore_string_instance(theme_name);
    theme = ecore_hash_get(efreet_icon_themes, key);
    if (!theme)
    {
        efreet_icon_theme_dir_scan_all(theme_name);
        theme = ecore_hash_get(efreet_icon_themes, key);
    }
    ecore_string_release(key);

    return theme;
}

/**
 * @internal
 * @param icon: The icon name to strip extension
 * @return Extension removed if in list of extensions, else untouched.
 * @brief Removes extension from icon name if in list of extensions.
 */
static char *
efreet_icon_remove_extension(const char *icon)
{
    char *tmp = NULL, *ext = NULL;

    tmp = strdup(icon);
    ext = strrchr(tmp, '.');
    if (ext)
    {
        const char *ext2;
        ecore_list_first_goto(efreet_icon_extensions);
        while ((ext2 = ecore_list_next(efreet_icon_extensions)))
        {
            if (!strcmp(ext, ext2))
            {
#ifdef STRICT_SPEC
                printf("[Efreet]: Requesting an icon with an extension: %s\n",
                                                                        icon);
#endif
                *ext = '\0';
                break;
            }
        }
    }

    return tmp;
}

/**
 * @internal
 * @param theme_name: The icon theme to look for
 * @return Returns the Efreet_Icon_Theme structure representing this theme
 * or a new blank theme if not found
 * @brief Retrieves a theme, or creates a blank one if not found.
 */
static Efreet_Icon_Theme *
efreet_icon_find_theme_check(const char *theme_name)
{
    Efreet_Icon_Theme *theme = NULL;
    theme = efreet_icon_theme_find(theme_name);
    if (!theme)
    {
        theme = efreet_icon_theme_new();
        theme->fake = 1;
        theme->name.internal = ecore_string_instance(theme_name);
        ecore_hash_set(efreet_icon_themes, (void *)theme->name.internal, theme);
    }

    return theme;
}

/**
 * @param theme_name: The icon theme to look for
 * @param icon: The icon to look for
 * @param size; The icon size to look for
 * @return Returns the path to the given icon or NULL if none found
 * @brief Retrives the path to the given icon.
 */
char *
efreet_icon_path_find(const char *theme_name, const char *icon, const char *size)
{
    struct stat st;
    char *value = NULL;
    char key[4096];
    Efreet_Icon_Cache *cache;
    Efreet_Icon_Theme *theme;

    snprintf(key, sizeof(key), "%s %s %s", theme_name, icon, size);
    cache = ecore_list_find(efreet_icon_cache, ECORE_COMPARE_CB(efreet_icon_cache_find), key);
    if (cache)
    {
        ecore_list_remove(efreet_icon_cache);
        if (!stat(cache->path, &st) && st.st_mtime == cache->lasttime)
        {
            ecore_list_prepend(efreet_icon_cache, cache);
            return strdup(cache->path);
        }
        else
            efreet_icon_cache_free(cache);
    }

    theme = efreet_icon_find_theme_check(theme_name);

#ifdef SLOPPY_SPEC
    {
        char *tmp;

        tmp = efreet_icon_remove_extension(icon);
        value = efreet_icon_find_helper(theme, tmp, size);
        FREE(tmp);
    }
#else
    value = efreet_icon_find_helper(theme, icon, size);
#endif

    /* we didn't find the icon in the theme or in the inherited directories
     * then just look for a non theme icon
     */
    if (!value) value = efreet_icon_fallback_icon(icon);

    if ((value) && !stat(value, &st))
    {
        Efreet_Icon_Cache *cache;

        cache = NEW(Efreet_Icon_Cache, 1);
        cache->key = strdup(key);
        cache->path = strdup(value);
        cache->lasttime = st.st_mtime;
        ecore_list_prepend(efreet_icon_cache, cache);
        efreet_icon_cache_flush();
    }

    return value;
}

/**
 * @param theme_name: The icon theme to look for
 * @param icon: List of icons to look for
 * @param size; The icon size to look for
 * @return Returns the path representing first found icon or
 * NULL if none of the icons are found
 * @brief Retrieves all of the information about the first found icon in
 * the list.
 * @note This function will search the given theme for all icons before falling
 * back. This is useful when searching for mimetype icons.
 */
char *
efreet_icon_list_find(const char *theme_name, Ecore_List *icons,
                                                            const char *size)
{
    const char *icon = NULL;
    char *value = NULL;
    Efreet_Icon_Theme *theme;

    theme = efreet_icon_find_theme_check(theme_name);

    ecore_list_first_goto(icons);
#ifdef SLOPPY_SPEC
    {
        Ecore_List *tmps = NULL;

        tmps = ecore_list_new();
        ecore_list_free_cb_set(tmps, free);
        ecore_list_first_goto(icons);
        while ((icon = ecore_list_next(icons)))
            ecore_list_append(tmps, efreet_icon_remove_extension(icon));

        value = efreet_icon_list_find_helper(theme, tmps, size);
        ecore_list_destroy(tmps);
    }
#else
    value = efreet_icon_list_find_helper(theme, icons, size);
#endif

    /* we didn't find the icons in the theme or in the inherited directories
     * then just look for a non theme icon
     */
    if (!value)
    {
        ecore_list_first_goto(icons);
        while ((icon = ecore_list_next(icons)))
        {
            if ((value = efreet_icon_fallback_icon(icon)))
                break;
        }
    }

    return value;
}

/**
 * @param theme: The icon theme to look for
 * @param icon: The icon to look for
 * @param size: The icon size to look for
 * @return Returns the Efreet_Icon structure representing this icon or NULL
 * if the icon is not found
 * @brief Retrieves all of the information about the given icon.
 */
Efreet_Icon *
efreet_icon_find(const char *theme, const char *icon, const char *size)
{
    char *path;

    path = efreet_icon_path_find(theme, icon, size);
    if (path)
    {
        Efreet_Icon *icon;

        icon = efreet_icon_new(path);
        free(path);
        return icon;
    }

    return NULL;
}

/**
 * @internal
 * @param theme: The theme to search in
 * @param icon: The icon to search for
 * @param size: The size to search for
 * @return Returns the icon matching the given information or NULL if no
 * icon found
 * @brief Scans inheriting themes for the given icon
 */
static char *
efreet_icon_find_fallback(Efreet_Icon_Theme *theme,
                          const char *icon, const char *size)
{
    char *parent = NULL;
    char *value = NULL;

    if (theme->inherits)
    {
        ecore_list_first_goto(theme->inherits);
        while ((parent = ecore_list_next(theme->inherits)))
        {
            Efreet_Icon_Theme *parent_theme;

            parent_theme = efreet_icon_theme_find(parent);
            if ((!parent_theme) || (parent_theme == theme)) continue;

            value = efreet_icon_find_helper(parent_theme, icon, size);
            if (value) break;
        }
    }
    /* if this isn't the hicolor theme, and we have no other fallbacks
     * check hicolor */
    else if (strcmp(theme->name.internal, "hicolor"))
    {
        Efreet_Icon_Theme *parent_theme;

        parent_theme = efreet_icon_theme_find("hicolor");
        if (parent_theme)
            value = efreet_icon_find_helper(parent_theme, icon, size);
    }

    return value;
}

/**
 * @internal
 * @param theme: The theme to search in
 * @param icon: The icon to search for
 * @param size: The size to search for
 * @return Returns the icon matching the given information or NULL if no
 * icon found
 * @brief Scans the theme and any inheriting themes for the given icon
 */
static char *
efreet_icon_find_helper(Efreet_Icon_Theme *theme,
                        const char *icon, const char *size)
{
    char *value;
    static int recurse = 0;

    efreet_icon_theme_cache_check(theme);

    /* go no further if this theme is fake */
    if (theme->fake || !theme->valid) return NULL;

    /* limit recursion in finding themes and inherited themes to 256 levels */
    if (recurse > 256) return NULL;
    recurse++;

    value = efreet_icon_lookup_icon(theme, icon, size);

    /* we didin't find the image check the inherited themes */
    if (!value)
        value = efreet_icon_find_fallback(theme, icon, size);

    recurse--;
    return value;
}

/**
 * @internal
 * @param theme: The theme to search in
 * @param icons: The icons to search for
 * @param size: The size to search for
 * @return Returns the icon matching the given information or NULL if no
 * icon found
 * @brief Scans inheriting themes for the given icons
 */
static char *
efreet_icon_list_find_fallback(Efreet_Icon_Theme *theme,
                               Ecore_List *icons, const char *size)
{
    char *parent = NULL;
    char *value = NULL;

    if (theme->inherits)
    {
        ecore_list_first_goto(theme->inherits);
        while ((parent = ecore_list_next(theme->inherits)))
        {
            Efreet_Icon_Theme *parent_theme;

            parent_theme = efreet_icon_theme_find(parent);
            if ((!parent_theme) || (parent_theme == theme)) continue;

            value = efreet_icon_list_find_helper(parent_theme,
                                                        icons, size);
            if (value) break;
        }
    }

    /* if this isn't the hicolor theme, and we have no other fallbacks
     * check hicolor
     */
    else if (strcmp(theme->name.internal, "hicolor"))
    {
        Efreet_Icon_Theme *parent_theme;

        parent_theme = efreet_icon_theme_find("hicolor");
        if (parent_theme)
            value = efreet_icon_list_find_helper(parent_theme,
                                                        icons, size);
    }

    return value;
}

/**
 * @internal
 * @param theme: The theme to search in
 * @param icons: The icons to search for
 * @param size: The size to search for
 * @return Returns the icon matching the given information or NULL if no
 * icon found
 * @brief Scans the theme and any inheriting themes for the given icons
 */
static char *
efreet_icon_list_find_helper(Efreet_Icon_Theme *theme,
                             Ecore_List *icons, const char *size)
{
    char *value = NULL;
    const char *icon = NULL;
    static int recurse = 0;

    efreet_icon_theme_cache_check(theme);

    /* go no further if this theme is fake */
    if (theme->fake || !theme->valid) return NULL;

    /* limit recursion in finding themes and inherited themes to 256 levels */
    if (recurse > 256) return NULL;
    recurse++;

    ecore_list_first_goto(icons);
    while ((icon = ecore_list_next(icons)))
    {
        if ((value = efreet_icon_lookup_icon(theme, icon, size)))
            break;
    }

    /* we didn't find the image check the inherited themes */
    if (!value)
        value = efreet_icon_list_find_fallback(theme, icons, size);

    recurse--;
    return value;
}

/**
 * @internal
 * @param theme: The icon theme to look in
 * @param icon_name: The icon name to look for
 * @param size: The icon size to look for
 * @return Returns the path for the theme/icon/size combo or NULL if
 * none found
 * @brief Looks for the @a icon in the @a theme for the @a size given.
 */
static char *
efreet_icon_lookup_icon(Efreet_Icon_Theme *theme, const char *icon_name,
                                                    const char *size)
{
    char *icon = NULL, *tmp = NULL;
    Efreet_Icon_Theme_Directory *dir;
    int minimal_size = INT_MAX;
    unsigned int real_size;

    if (!theme || (theme->paths.count == 0) || !icon_name || !size)
        return NULL;

    real_size = atoi(size);

    /* search for allowed size == requested size */
    ecore_list_first_goto(theme->directories);
    while ((dir = ecore_list_next(theme->directories)))
    {
        if (!efreet_icon_directory_size_match(dir, real_size)) continue;
        icon = efreet_icon_lookup_directory(theme, dir,
                                            icon_name);
        if (icon) return icon;
    }

    /* search for any icon that matches */
    ecore_list_first_goto(theme->directories);
    while ((dir = ecore_list_next(theme->directories)))
    {
        int distance;

        distance = efreet_icon_directory_size_distance(dir, real_size);
        if (distance >= minimal_size) continue;

        tmp = efreet_icon_lookup_directory(theme, dir,
                                           icon_name);
        if (tmp)
        {
            FREE(icon);
            icon = tmp;
            minimal_size = distance;
        }
    }

    return icon;
}


/**
 * @internal
 * @param theme: The theme to use
 * @param dir: The theme directory to look in
 * @param icon_name: The icon name to look for
 * @param size: The icon size to look for
 * @return Returns the icon cloest matching the given information or NULL if
 * none found
 * @brief Tries to find the file closest matching the given icon
 */
static char *
efreet_icon_lookup_directory(Efreet_Icon_Theme *theme,
                             Efreet_Icon_Theme_Directory *dir,
                             const char *icon_name)
{
    if (theme->paths.count == 1)
        return efreet_icon_lookup_directory_helper(dir, theme->paths.path, icon_name);

    else
    {
        char *icon;
        const char *path;

        ecore_list_first_goto(theme->paths.path);
        while ((path = ecore_list_next(theme->paths.path)))
        {
            icon = efreet_icon_lookup_directory_helper(dir, path, icon_name);
            if (icon) return icon;
        }
    }

    return NULL;
}

/**
 * @internal
 * @param dir: The theme directory to work with
 * @param size: The size to check
 * @return Returns true if the size matches for the given directory, 0
 * otherwise
 * @brief Checks if the size matches for the given directory or not
 */
static int
efreet_icon_directory_size_match(Efreet_Icon_Theme_Directory *dir,
                                unsigned int size)
{
    if (dir->type == EFREET_ICON_SIZE_TYPE_FIXED)
        return (dir->size.normal == size);

    if (dir->type == EFREET_ICON_SIZE_TYPE_SCALABLE)
        return ((dir->size.min < size) && (size < dir->size.max));

    if (dir->type == EFREET_ICON_SIZE_TYPE_THRESHOLD)
        return (((dir->size.normal - dir->size.threshold) < size)
                && (size < (dir->size.normal + dir->size.threshold)));

    return 0;
}

/**
 * @internal
 * @param dir: The directory to work with
 * @param size: The size to check for
 * @return Returns the distance this size is away from the desired size
 * @brief Returns the distance the given size is away from the desired size
 */
static int
efreet_icon_directory_size_distance(Efreet_Icon_Theme_Directory *dir,
                                    unsigned int size)
{
    if (dir->type == EFREET_ICON_SIZE_TYPE_FIXED)
        return (abs(dir->size.normal - size));

    if (dir->type == EFREET_ICON_SIZE_TYPE_SCALABLE)
    {
        if (size < dir->size.min)
            return dir->size.min - size;
        if (dir->size.max < size)
            return size - dir->size.max;

        return 0;
    }

    if (dir->type == EFREET_ICON_SIZE_TYPE_THRESHOLD)
    {
        if (size < (dir->size.normal - dir->size.threshold))
            return (dir->size.min - size);
        if ((dir->size.normal + dir->size.threshold) < size)
            return (size - dir->size.max);

        return 0;
    }

    return 0;
}

/**
 * @internal
 * @param icon_name: The icon name to look for
 * @return Returns the Efreet_Icon for the given name or NULL if none found
 * @brief Looks for the un-themed icon in the base directories
 */
static char *
efreet_icon_fallback_icon(const char *icon_name)
{
    char *icon;

    if (!icon_name) return NULL;

    icon = efreet_icon_fallback_dir_scan(efreet_icon_deprecated_user_dir_get(), icon_name);
    if (!icon)
        icon = efreet_icon_fallback_dir_scan(efreet_icon_user_dir_get(), icon_name);
    if (!icon)
    {
        Ecore_List *xdg_dirs;
        const char *dir;
        char path[PATH_MAX];

        ecore_list_first_goto(efreet_extra_icon_dirs);
        while ((dir = ecore_list_next(efreet_extra_icon_dirs)))
        {
            icon = efreet_icon_fallback_dir_scan(dir, icon_name);
            if (icon) return icon;
        }

        xdg_dirs = efreet_data_dirs_get();
        ecore_list_first_goto(xdg_dirs);
        while ((dir = ecore_list_next(xdg_dirs)))
        {
            snprintf(path, PATH_MAX, "%s/icons", dir);
            icon = efreet_icon_fallback_dir_scan(path, icon_name);
            if (icon) return icon;
        }

        icon = efreet_icon_fallback_dir_scan("/usr/share/pixmaps", icon_name);
    }

    return icon;
}

/**
 * @internal
 * @param dir: The directory to scan
 * @param icon_name: The icon to look for
 * @return Returns the icon for the given name or NULL on failure
 * @brief Scans the given @a dir for the given @a icon_name returning the
 * Efreet_icon if found, NULL otherwise.
 */
static char *
efreet_icon_fallback_dir_scan(const char *dir, const char *icon_name)
{
    char *icon = NULL;
    char path[PATH_MAX], *ext;

    if (!dir || !icon_name) return NULL;

    ecore_list_first_goto(efreet_icon_extensions);
    while ((ext = ecore_list_next(efreet_icon_extensions)))
    {
        const char *icon_path[] = { dir, "/", icon_name, ext, NULL };
        efreet_array_cat(path, sizeof(path), icon_path);

        if (ecore_file_exists(path))
        {
            icon = strdup(path);
            if (icon) break;
        }
    }
    /* This is to catch non-conforming .desktop files */
#ifdef SLOPPY_SPEC
    if (!icon)
    {
        const char *icon_path[] = { dir, "/", icon_name, NULL };
        efreet_array_cat(path, sizeof(path), icon_path);

        if (ecore_file_exists(path))
        {
            icon = strdup(path);
#ifdef STRICT_SPEC
            if (icon)
                printf("[Efreet]: Found an icon that already has an extension: %s\n", path);
#endif
        }
    }
#endif

    return icon;
}

/**
 * @internal
 * @param theme: The theme to work with
 * @param dir: The theme directory to work with
 * @param path: The partial path to use
 * @return Returns no value
 * @brief Caches the icons in the given theme directory path at the given
 * size
 */
static char *
efreet_icon_lookup_directory_helper(Efreet_Icon_Theme_Directory *dir,
                                    const char *path, const char *icon_name)
{
    char *icon = NULL;
    char file_path[PATH_MAX];
    const char *ext, *path_strs[] = { path, "/", dir->name, "/", icon_name, NULL, NULL };

    ecore_list_first_goto(efreet_icon_extensions);
    while ((ext = ecore_list_next(efreet_icon_extensions)))
    {
        path_strs[5] = ext;
        efreet_array_cat(file_path, sizeof(file_path), path_strs);

        if (ecore_file_exists(file_path))
        {
            icon = strdup(file_path);
            break;
        }
    }
    return icon;
}

/**
 * @internal
 * @return Returns a new Efreet_Icon struct on success or NULL on failure
 * @brief Creates a new Efreet_Icon struct
 */
static Efreet_Icon *
efreet_icon_new(const char *path)
{
    Efreet_Icon *icon;
    char *p;

    icon = NEW(Efreet_Icon, 1);
    icon->path = strdup(path);

    /* load the .icon file if it's available */
    p = strrchr(icon->path, '.');
    if (p)
    {
        char ico_path[PATH_MAX];

        *p = '\0';

        snprintf(ico_path, sizeof(ico_path), "%s.icon", icon->path);
        *p = '.';

        if (ecore_file_exists(ico_path))
            efreet_icon_populate(icon, ico_path);
    }

    if (!icon->name)
    {
        const char *file;

        file = ecore_file_file_get(icon->path);
        p = strrchr(icon->path, '.');
        if (p) *p = '\0';
        icon->name = strdup(file);
        if (p) *p = '.';
    }

    return icon;
}

/**
 * @param icon: The Efreet_Icon to cleanup
 * @return Returns no value.
 * @brief Free's the given icon and all its internal data.
 */
void
efreet_icon_free(Efreet_Icon *icon)
{
    if (!icon) return;

    icon->ref_count --;
    if (icon->ref_count > 0) return;

    IF_FREE(icon->path);
    IF_FREE(icon->name);
    IF_FREE_LIST(icon->attach_points);

    FREE(icon);
}

/**
 * @internal
 * @param point: The Efreet_Icon_Point to free
 * @return Returns no value
 * @brief Frees the given structure
 */
static void
efreet_icon_point_free(Efreet_Icon_Point *point)
{
    if (!point) return;

    FREE(point);
}

/**
 * @internal
 * @param icon: The icon to populate
 * @param file: The file to populate from
 * @return Returns no value
 * @brief Tries to populate the icon information from the given file
 */
static void
efreet_icon_populate(Efreet_Icon *icon, const char *file)
{
    Efreet_Ini *ini;
    const char *tmp;

    ini = efreet_ini_new(file);
    if (!ini->data)
    {
        efreet_ini_free(ini);
        return;
    }

    efreet_ini_section_set(ini, "Icon Data");
    tmp = efreet_ini_localestring_get(ini, "DisplayName");
    if (tmp) icon->name = strdup(tmp);

    tmp = efreet_ini_string_get(ini, "EmbeddedTextRectangle");
    if (tmp)
    {
        int points[4];
        char *t, *s, *p;
        int i;

        t = strdup(tmp);
        s = t;
        for (i = 0; i < 4; i++)
        {
            if (s)
            {
                p = strchr(s, ',');

                if (p) *p = '\0';
                points[i] = atoi(s);

                if (p) s = ++p;
                else s = NULL;
            }
            else
            {
                points[i] = 0;
            }
        }

        icon->has_embedded_text_rectangle = 1;
        icon->embedded_text_rectangle.x0 = points[0];
        icon->embedded_text_rectangle.y0 = points[1];
        icon->embedded_text_rectangle.x1 = points[2];
        icon->embedded_text_rectangle.y1 = points[3];

        FREE(t);
    }

    tmp = efreet_ini_string_get(ini, "AttachPoints");
    if (tmp)
    {
        char *t, *s, *p;

        icon->attach_points = ecore_list_new();
        ecore_list_free_cb_set(icon->attach_points,
                            ECORE_FREE_CB(efreet_icon_point_free));

        t = strdup(tmp);
        s = t;
        p = t;
        while (s)
        {
            Efreet_Icon_Point *point;

            p = strchr(s, ',');
            /* If this happens there is something wrong with the .icon file */
            if (!p) break;

            point = NEW(Efreet_Icon_Point, 1);

            *p = '\0';
            point->x = atoi(s);

            s = ++p;
            p = strchr(s, '|');
            if (p) *p = '\0';

            point->y = atoi(s);
            ecore_list_append(icon->attach_points, point);

            if (p) s = ++p;
            else s = NULL;
        }
        FREE(t);
    }

    efreet_ini_free(ini);
}

/**
 * @internal
 * @return Returns a new Efreet_Icon_Theme on success or NULL on failure
 * @brief Creates a new Efreet_Icon_Theme structure
 */
static Efreet_Icon_Theme *
efreet_icon_theme_new(void)
{
    Efreet_Icon_Theme *theme;

    theme = NEW(Efreet_Icon_Theme, 1);

    return theme;
}

/**
 * @internal
 * @param theme: The theme to free
 * @return Returns no value
 * @brief Frees up the @a theme structure.
 */
static void
efreet_icon_theme_free(Efreet_Icon_Theme *theme)
{
    if (!theme) return;

    IF_RELEASE(theme->name.internal);
    IF_RELEASE(theme->name.name);

    IF_FREE(theme->comment);
    IF_FREE(theme->example_icon);

    if (theme->paths.count == 1)
        IF_FREE(theme->paths.path);
    else
        IF_FREE_LIST(theme->paths.path);

    IF_FREE_LIST(theme->inherits);
    IF_FREE_LIST(theme->directories);

    FREE(theme);
}

/**
 * @internal
 * @param theme: The theme to work with
 * @param path: The path to add
 * @return Returns no value
 * @brief This will correctly add the given path to the list of theme paths.
 * @Note Assumes you've already verified that @a path is a valid directory.
 */
static void
efreet_icon_theme_path_add(Efreet_Icon_Theme *theme, const char *path)
{
    if (!theme || !path) return;

    if (theme->paths.count == 0)
        theme->paths.path = strdup(path);

    else if (theme->paths.count > 1)
        ecore_list_append(theme->paths.path, strdup(path));

    else
    {
        char *old;

        old = theme->paths.path;
        theme->paths.path = ecore_list_new();
        ecore_list_free_cb_set(theme->paths.path, free);

        ecore_list_append(theme->paths.path, old);
        ecore_list_append(theme->paths.path, strdup(path));
    }
    theme->paths.count++;
}

/**
 * @internal
 * @return Returns no value
 * @brief This validates that our cache is still valid.
 *
 * This is checked by the following algorithm:
 *   - if we've check less then 5 seconds ago we're good
 *   - if the mtime on the dir is less then our last check time we're good
 *   - otherwise, invalidate the caches
 */
static void
efreet_icon_theme_cache_check(Efreet_Icon_Theme *theme)
{
    double new_check;

    new_check = ecore_time_get();

    /* we're within 5 seconds of the last time we checked the cache */
    if ((new_check - 5) <= theme->last_cache_check) return;

    if (theme->fake)
        efreet_icon_theme_dir_scan_all(theme->name.internal);

    else if (theme->paths.count == 1)
        efreet_icon_theme_cache_check_dir(theme, theme->paths.path);

    else if (theme->paths.count > 1)
    {
        char *path;

        ecore_list_first_goto(theme->paths.path);
        while ((path = ecore_list_next(theme->paths.path)))
        {
            if (!efreet_icon_theme_cache_check_dir(theme, path))
                break;
        }
    }
}

/**
 * @internal
 * @param theme: The icon theme to check
 * @param dir: The directory to check
 * @return Returns 1 if the cache is still valid, 0 otherwise
 * @brief This will check if the theme cache is still valid. If it isn't the
 * cache will be invalided and 0 returned.
 */
static int
efreet_icon_theme_cache_check_dir(Efreet_Icon_Theme *theme, const char *dir)
{
    struct stat buf;

    /* have we modified this directory since our last cache check? */
    if (stat(dir, &buf) || (buf.st_mtime > theme->last_cache_check))
    {
        if (efreet_icon_dirs_cached)
            ecore_hash_remove(efreet_icon_dirs_cached, dir);
        return 0;
    }

    return 1;
}

/**
 * @internal
 * @param theme_name: The theme to scan for
 * @return Returns no value
 * @brief Scans the theme directories. If @a theme_name is NULL it will load
 * up all theme data. If @a theme_name is not NULL it will look for that
 * specific theme data
 */
static void
efreet_icon_theme_dir_scan_all(const char *theme_name)
{
    Ecore_List *xdg_dirs;
    char path[PATH_MAX], *dir;

    efreet_icon_theme_dir_scan(efreet_icon_deprecated_user_dir_get(), theme_name);
    efreet_icon_theme_dir_scan(efreet_icon_user_dir_get(), theme_name);

    xdg_dirs = efreet_data_dirs_get();
    ecore_list_first_goto(xdg_dirs);
    while ((dir = ecore_list_next(xdg_dirs)))
    {
        snprintf(path, sizeof(path), "%s/icons", dir);
        efreet_icon_theme_dir_scan(path, theme_name);
    }

    efreet_icon_theme_dir_scan("/usr/share/pixmaps", theme_name);
}

/**
 * @internal
 * @param search_dir: The directory to scan
 * @param theme_name: Scan for this specific theme, set to NULL to find all
 * themes.
 * @return Returns no value
 * @brief Scans the given directory and adds non-hidden icon themes to the
 * given list. If the theme isnt' in our cache then load the index.theme and
 * add to the cache.
 */
static void
efreet_icon_theme_dir_scan(const char *search_dir, const char *theme_name)
{
    DIR *dirs;
    struct dirent *dir;

    if (!search_dir) return;

    dirs = opendir(search_dir);
    if (!dirs) return;

    while ((dir = readdir(dirs)))
    {
        Efreet_Icon_Theme *theme;
        char path[PATH_MAX];
        const char *key;

        if (!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, "..")) continue;

        /* only care if this is a directory or the theme name matches the
         * given name */
        snprintf(path, sizeof(path), "%s/%s", search_dir, dir->d_name);
        if (((theme_name != NULL) && (strcmp(theme_name, dir->d_name)))
                || !ecore_file_is_dir(path))
            continue;

        key = ecore_string_instance(dir->d_name);
        theme = ecore_hash_get(efreet_icon_themes, key);

        if (!theme)
        {
            theme = efreet_icon_theme_new();
            theme->name.internal = key;
            ecore_hash_set(efreet_icon_themes,
                        (void *)theme->name.internal, theme);
        }
        else
        {
            if (theme->fake)
                theme->fake = 0;
            ecore_string_release(key);
        }

        efreet_icon_theme_path_add(theme, path);

        /* we're already valid so no reason to check for an index.theme file */
        if (theme->valid) continue;

        /* if the index.theme file exists we parse it into the theme */
        strncat(path, "/index.theme", sizeof(path));
        if (ecore_file_exists(path))
            efreet_icon_theme_index_read(theme, path);
    }
    closedir(dirs);

    /* if we were given a theme name we want to make sure that that given
     * theme is valid before finishing, unless it's a fake theme */
    if (theme_name)
    {
        Efreet_Icon_Theme *theme;

        theme = ecore_hash_get(efreet_icon_themes, theme_name);
        if (theme && !theme->valid && !theme->fake)
        {
            ecore_hash_remove(efreet_icon_themes, theme_name);
            efreet_icon_theme_free(theme);
        }
    }
}

/**
 * @internal
 * @param theme: The theme to set the values into
 * @param path: The path to the index.theme file for this theme
 * @return Returns no value
 * @brief This will load up the theme with the data in the index.theme file
 */
static void
efreet_icon_theme_index_read(Efreet_Icon_Theme *theme, const char *path)
{
    Efreet_Ini *ini;
    const char *tmp;

    if (!theme || !path) return;

    ini = efreet_ini_new(path);
    if (!ini->data)
    {
        efreet_ini_free(ini);
        return;
    }

    efreet_ini_section_set(ini, "Icon Theme");
    tmp = efreet_ini_localestring_get(ini, "Name");
    if (tmp) theme->name.name = ecore_string_instance(tmp);

    tmp = efreet_ini_localestring_get(ini, "Comment");
    if (tmp) theme->comment = strdup(tmp);

    tmp = efreet_ini_string_get(ini, "Example");
    if (tmp) theme->example_icon = strdup(tmp);

    theme->hidden = efreet_ini_boolean_get(ini, "Hidden");

    theme->valid = 1;

    /* Check the inheritance. If there is none we inherit from the hicolor theme */
    tmp = efreet_ini_string_get(ini, "Inherits");
    if (tmp)
    {
        char *t, *s, *p;

        theme->inherits = ecore_list_new();
        ecore_list_free_cb_set(theme->inherits, free);

        t = strdup(tmp);
        s = t;
        p = strchr(s, ',');

        while (p)
        {
            *p = '\0';

            ecore_list_append(theme->inherits, strdup(s));
            s = ++p;
            p = strchr(s, ',');
        }
        ecore_list_append(theme->inherits, strdup(s));

        FREE(t);
    }

    /* make sure this one is done last as setting the directory will change
     * the ini section ... */
    tmp = efreet_ini_string_get(ini, "Directories");
    if (tmp)
    {
        char *t, *s, *p;
        int last = 0;

        theme->directories = ecore_list_new();
        ecore_list_free_cb_set(theme->directories,
                            ECORE_FREE_CB(efreet_icon_theme_directory_free));

        t = strdup(tmp);
        s = t;
        p = s;

        while (!last)
        {
            p = strchr(s, ',');

            if (!p) last = 1;
            else *p = '\0';

            ecore_list_append(theme->directories,
                            efreet_icon_theme_directory_new(ini, s));

            if (!last) s = ++p;
        }

        FREE(t);
    }

    efreet_ini_free(ini);
}

/**
 * @internal
 * @return Returns no value
 * @brief Because the theme icon directories can be spread over multiple
 * base directories we may need to create the icon theme strucutre before
 * finding the index.theme file. It may also be that we never find an
 * index.theme file as this isn't a valid theme. This function makes sure
 * that everything we've got in our hash has a valid key to it.
 */
static void
efreet_icon_theme_dir_validity_check(void)
{
    Ecore_List *keys;
    const char *name;

    keys = ecore_hash_keys(efreet_icon_themes);
    ecore_list_first_goto(keys);
    while ((name = ecore_list_next(keys)))
    {
        Efreet_Icon_Theme *theme;

        theme = ecore_hash_get(efreet_icon_themes, name);
        if (!theme->valid && !theme->fake)
        {
            ecore_hash_remove(efreet_icon_themes, name);
            efreet_icon_theme_free(theme);
        }
    }
    ecore_list_destroy(keys);
}

/**
 * @internal
 * @param ini: The ini file with information on this directory
 * @param name: The name of the directory
 * @return Returns a new Efreet_Icon_Theme_Directory based on the
 * information in @a ini.
 * @brief Creates and initialises an icon theme directory from the given ini
 * information
 */
static Efreet_Icon_Theme_Directory *
efreet_icon_theme_directory_new(Efreet_Ini *ini, const char *name)
{
    Efreet_Icon_Theme_Directory *dir;
    int val;
    const char *tmp;

    if (!ini) return NULL;

    dir = NEW(Efreet_Icon_Theme_Directory, 1);
    dir->name = strdup(name);

    efreet_ini_section_set(ini, name);

    tmp = efreet_ini_string_get(ini, "Context");
    if (tmp)
    {
        if (!strcasecmp(tmp, "Actions"))
            dir->context = EFREET_ICON_THEME_CONTEXT_ACTIONS;

        else if (!strcasecmp(tmp, "Devices"))
            dir->context = EFREET_ICON_THEME_CONTEXT_DEVICES;

        else if (!strcasecmp(tmp, "FileSystems"))
            dir->context = EFREET_ICON_THEME_CONTEXT_FILESYSTEMS;

        else if (!strcasecmp(tmp, "MimeTypes"))
            dir->context = EFREET_ICON_THEME_CONTEXT_MIMETYPES;
    }

    tmp = efreet_ini_string_get(ini, "Type");
    if (tmp)
    {
        if (!strcasecmp(tmp, "Fixed"))
            dir->type = EFREET_ICON_SIZE_TYPE_FIXED;

        else if (!strcasecmp(tmp, "Scalable"))
            dir->type = EFREET_ICON_SIZE_TYPE_SCALABLE;

        else if (!strcasecmp(tmp, "Threshold"))
            dir->type = EFREET_ICON_SIZE_TYPE_THRESHOLD;
    }

    dir->size.normal = efreet_ini_int_get(ini, "Size");

    val = efreet_ini_int_get(ini, "MinSize");
    if (val < 0) dir->size.min = dir->size.normal;
    else dir->size.min = val;

    val = efreet_ini_int_get(ini, "MaxSize");
    if (val < 0) dir->size.max = dir->size.normal;
    else dir->size.max = val;

    val = efreet_ini_int_get(ini, "Threshold");
    if (val < 0) dir->size.threshold = 2;
    else dir->size.threshold = val;

    return dir;
}

/**
 * @internal
 * @param dir: The Efreet_Icon_Theme_Directory to free
 * @return Returns no value
 * @brief Frees the given directory @a dir
 */
static void
efreet_icon_theme_directory_free(Efreet_Icon_Theme_Directory *dir)
{
    if (!dir) return;

    IF_FREE(dir->name);
    FREE(dir);
}

static int
efreet_icon_cache_find(Efreet_Icon_Cache *value, const char *key)
{
    if (!value || !key) return -1;
    return strcmp(value->key, key);
}

static void
efreet_icon_cache_flush(void)
{
    /* TODO:
     * * Dynamic cache size
     * * Maybe add references to cache, so that we sort on how often a value is used
     */
    while (ecore_list_count(efreet_icon_cache) > 100)
    {
        Efreet_Icon_Cache *cache;

        cache = ecore_list_last_remove(efreet_icon_cache);
        efreet_icon_cache_free(cache);
    }
}

static void
efreet_icon_cache_free(Efreet_Icon_Cache *value)
{
    if (!value) return;

    IF_FREE(value->key);
    IF_FREE(value->path);
    free(value);
}

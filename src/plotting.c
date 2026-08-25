#include "epidemic.h"

#include <stdlib.h>
#include <string.h>

static void set_error(char *error, size_t error_size, const char *message)
{
    if (error != NULL && error_size > 0U) {
        (void)snprintf(error, error_size, "%s", message);
    }
}

static const char *compartment_short_name(ModelType model, int compartment)
{
    if (compartment == 0) {
        return "S";
    }
    if (model == MODEL_SIR) {
        return compartment == 1 ? "I" : "R";
    }
    if (compartment == 1) {
        return "E";
    }
    if (compartment == 2) {
        return "I";
    }
    if (model == MODEL_SEIHRS && compartment == 3) {
        return "H";
    }
    return "R";
}

static const char *compartment_color(ModelType model, int compartment)
{
    static const char *const colors[] = {
        "#2ca02c", "#ff8c00", "#d62728", "#9467bd", "#1f77b4"
    };
    int color_index = compartment;
    if (model == MODEL_SIR && compartment > 0) {
        color_index = compartment == 1 ? 2 : 4;
    } else if (model == MODEL_SEIR && compartment == 3) {
        color_index = 4;
    }
    return colors[color_index];
}

static int write_series(FILE *file, const char *data_path, int column,
                        const char *style, const char *color, const char *label,
                        size_t population, size_t input_count, int replicates, int last)
{
    char title[16];
    int title_written;
    title_written = snprintf(title, sizeof(title), "%s%zu", label, population + 1U);
    if (title_written < 0 || (size_t)title_written >= sizeof(title)) {
        return 0;
    }
    if (input_count == 1U) {
        (void)snprintf(title, sizeof(title), "%s", label);
    }
    if (replicates > 1) {
        if (fprintf(file,
                    "  '%s' index 0 using 1:%d with lines %s lc rgb '%s' title '%s', \\\n",
                    data_path, column, style, color, title) < 0 ||
            fprintf(file,
                    "  for [rep=1:%d] '%s' index rep using 1:%d with lines %s lc rgb '%s' notitle",
                    replicates - 1, data_path, column, style, color) < 0) {
            return 0;
        }
    } else if (fprintf(file,
                       "  '%s' using 1:%d with lines %s lc rgb '%s' title '%s'",
                       data_path, column, style, color, title) < 0) {
        return 0;
    }
    if (!last && fprintf(file, ", \\\n") < 0) {
        return 0;
    }
    return 1;
}

static int write_panel(FILE *file, const char *data_path, ModelType model,
                       size_t input_count, int replicates, const int *indices,
                       int index_count, const char *title, int show_xlabel,
                       double key_y)
{
    int compartments = model == MODEL_SIR ? 3 : (model == MODEL_SEIR ? 4 : 5);
    size_t population;
    int item;
    int total_items = (int)input_count * index_count;
    int item_number = 0;
    if (fprintf(file, "set key at screen 0.50,%0.3f center bottom\nset key maxrows 2\n", key_y) < 0) {
        return 0;
    }
    if (title[0] != '\0' && fprintf(file, "set title '%s' offset 0,-1\n", title) < 0) {
        return 0;
    }
    if (show_xlabel) {
        if (fprintf(file, "set xlabel 'Days'\n") < 0) {
            return 0;
        }
    } else if (fprintf(file, "unset xlabel\n") < 0) {
        return 0;
    }
    if (fprintf(file, "plot \\\n") < 0) {
        return 0;
    }
    for (population = 0U; population < input_count; ++population) {
        const char *style = population == 0U ?
            (replicates > 1 ? "lt 1 lw 1" : "lt 1 lw 2") :
            (replicates > 1 ? "dt 2 lw 1" : "dt 2 lw 2");
        for (item = 0; item < index_count; ++item) {
            int compartment = indices[item];
            int column = 2 + (int)population * compartments + compartment;
            int last = item_number == total_items - 1;
            if (!write_series(file, data_path, column, style,
                              compartment_color(model, compartment),
                              compartment_short_name(model, compartment),
                              population, input_count, replicates, last)) {
                return 0;
            }
            ++item_number;
        }
    }
    return fputc('\n', file) != EOF;
}

static int write_common_settings(FILE *file, int save_png)
{
    if (save_png) {
        if (fprintf(file,
                    "set terminal pngcairo size 1600,1000 enhanced font 'Arial,11'\n"
                    "set output 'output/plots/epidemic.png'\n") < 0) {
            return 0;
        }
    } else if (fprintf(file,
                       "set terminal qt size 1400,900 enhanced font 'Arial,10'\n") < 0) {
        return 0;
    }
    return fprintf(file,
                   "set border linewidth 1\n"
                   "set tics out\n"
                   "set format y '%%.0f'\n"
                   "set grid\n"
                   "set ylabel 'Number of individuals'\n"
                   "set key horizontal\n"
                   "set key top center\n"
                   "set key samplen 2\n"
                   "set key spacing 1\n"
                   "set key font ',9'\n"
                   "set lmargin 10\n"
                   "set rmargin 4\n") >= 0;
}

int write_plot_script(const char *script_path, const char *data_path,
                      ModelType model, size_t input_count, int replicates,
                      int save_png, char *error, size_t error_size)
{
    FILE *file;
    int all_indices[] = {0, 1, 2, 3, 4};
    int active_indices[] = {1, 2, 3};
    int all_count;
    int active_count;
    const char *main_title;
    if (script_path == NULL || data_path == NULL || input_count < 1U || input_count > 2U ||
        replicates <= 0 || model < MODEL_SIR || model > MODEL_SEIHRS) {
        set_error(error, error_size, "Invalid plot script arguments");
        return 0;
    }
    file = fopen(script_path, "w");
    if (file == NULL) {
        set_error(error, error_size, "Could not create Gnuplot script");
        return 0;
    }
    all_count = model == MODEL_SIR ? 3 : (model == MODEL_SEIR ? 4 : 5);
    active_count = model == MODEL_SEIR ? 2 : 3;
    main_title = model == MODEL_SIR ? "SIR epidemic model" :
                 (model == MODEL_SEIR ? "SEIR epidemic model" : "SEIHRS epidemic model");
    if (!write_common_settings(file, save_png) ||
        (model != MODEL_SIR &&
         fprintf(file, "set label 99 '%s' at screen 0.5,0.975 center font ',14'\n",
                 main_title) < 0)) {
        (void)fclose(file);
        set_error(error, error_size, "Could not write Gnuplot configuration");
        return 0;
    }
    if (model == MODEL_SIR) {
        if (!write_panel(file, data_path, model, input_count, replicates,
                         all_indices, all_count, main_title, 1, 0.920)) {
            (void)fclose(file);
            set_error(error, error_size, "Could not write Gnuplot data columns");
            return 0;
        }
    } else {
        if (fprintf(file,
                    "set multiplot\n"
                    "set origin 0.08,0.54\n"
                    "set size 0.84,0.36\n"
                    "set tmargin 2\n"
                    "set bmargin 1\n") < 0 ||
            !write_panel(file, data_path, model, input_count, replicates,
                         all_indices, all_count, "All compartments", 0, 0.920) ||
            fprintf(file,
                    "unset title\n"
                    "unset xlabel\n"
                    "set origin 0.08,0.08\n"
                    "set size 0.84,0.36\n"
                    "set tmargin 2\n"
                    "set bmargin 5\n") < 0 ||
            !write_panel(file, data_path, model, input_count, replicates,
                         active_indices, active_count, "Active outbreak compartments", 1, 0.470) ||
            fprintf(file, "unset title\nunset key\nunset label 99\nunset multiplot\n") < 0) {
            (void)fclose(file);
            set_error(error, error_size, "Could not write Gnuplot panels");
            return 0;
        }
    }
    if (model == MODEL_SIR && fprintf(file, "unset title\nunset key\n") < 0) {
        (void)fclose(file);
        set_error(error, error_size, "Could not finish Gnuplot script");
        return 0;
    }
    if (fclose(file) != 0) {
        set_error(error, error_size, "Could not finish Gnuplot script");
        return 0;
    }
    return 1;
}

int gnuplot_available(void)
{
#ifdef _WIN32
    return system("where gnuplot >NUL 2>NUL") == 0;
#else
    return system("command -v gnuplot >/dev/null 2>&1") == 0;
#endif
}

int launch_gnuplot(const char *script_path)
{
    char command[EPIDEMIC_MAX_PATH + 64];
    if (script_path == NULL || strlen(script_path) >= EPIDEMIC_MAX_PATH) {
        return 0;
    }
#ifdef _WIN32
    (void)snprintf(command, sizeof(command), "gnuplot -persist \"%s\"", script_path);
#else
    (void)snprintf(command, sizeof(command), "gnuplot -persist '%s'", script_path);
#endif
    return system(command) == 0;
}

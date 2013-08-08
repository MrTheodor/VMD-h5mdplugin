/***************************************************************************
 *
 *            (C) Copyright 2013
 *
 ***************************************************************************/

/***************************************************************************
 * RCS INFORMATION:
 *
 *      $RCSfile: xyzplugin.c,v $
 *      $Author: Jonas Landsgesell, Sascha Ehrhardt S$
 *      $Revision: 1.0 $       $Date: 2013/08/01 14:00:00 $
 *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "molfile_plugin.h"
#include "hdf5.h"


typedef struct {
	int ntime;
	int natoms;
	int nspacedims;
	molfile_atom_t *atomlist;
	char *file_name;
	hid_t file_id;
	hid_t dataset_id;
} h5mddata;

int reads = 0;
int current_time = 0;

static void get_xyz(void *mydata, int atom_nr, int time_i,
		double xyz_array[3]) {
	h5mddata *data = (h5mddata *) mydata;
	double data_out[data->ntime][data->natoms][data->nspacedims];
	H5Dread(data->dataset_id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, data_out);

	xyz_array[0] = data_out[time_i][atom_nr][0];
	xyz_array[1] = data_out[time_i][atom_nr][1];
	xyz_array[2] = data_out[time_i][atom_nr][2];
}

static void *open_h5md_read(const char *filename, const char *filetype, int *natoms) {
	h5mddata *data;
	data = (h5mddata *) malloc(sizeof(h5mddata));

	hid_t file_id = H5Fopen(filename, H5F_ACC_RDWR, H5P_DEFAULT);
	hid_t dataset_id = H5Dopen(file_id, "/trajectory/atoms/position/value",H5P_DEFAULT);
	hid_t dataspace = H5Dget_space(dataset_id);

	unsigned long long int dims_out[3];
	H5Sget_simple_extent_dims(dataspace, dims_out, NULL );

	data->file_name = filename;
	data->ntime = dims_out[0];
	data->natoms = dims_out[1];
	*natoms = data->natoms;
	data->nspacedims = dims_out[2];
	data->dataset_id=dataset_id;

	return data;

}

static int read_h5md_structure(void *mydata, int *optflags,molfile_atom_t *atoms) {
	/////////////////////// TODO READ SPECIES INFO (specification needed)
	molfile_atom_t *atom;
	h5mddata *data = (h5mddata *) mydata;

	*optflags = MOLFILE_ATOMICNUMBER | MOLFILE_MASS | MOLFILE_RADIUS;

	double data_out[data->ntime][data->natoms][data->nspacedims];
	H5Dread(data->dataset_id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, data_out);

	for (int i = 0; i < data->natoms; i++) {
		atom = atoms + i;
		int idx;
		idx = (int) data_out[20][i][0];
		char const dummy_label[16];
		const char *pdummy_label=dummy_label;
		char dummy_name[16]="XXX";


		strncpy(dummy_name, pdummy_label, sizeof(dummy_name));
		atom->atomicnumber = idx;
		float dummy_mass=1.0;
		atom->mass = dummy_mass;
		float dummy_radius=0.5;
		atom->radius = dummy_radius;
		strncpy(atom->type, atom->name, sizeof(atom->type));

		atom->resname[0] = '\0';
		atom->resid = 1;
		atom->chain[0] = '\0';
		atom->segid[0] = '\0';
	}

	return MOLFILE_SUCCESS;
}

static int read_h5md_timestep(void *mydata, int natoms, molfile_timestep_t *ts) {
	float x, y, z;

	h5mddata *data = (h5mddata *) mydata;

	/* read the coordinates */
	unsigned int ntime = data->ntime;
	if (current_time >= ntime - 1) {
		return MOLFILE_ERROR;
	}
	for (int i = 0; i < natoms; i++) {

		current_time = reads / natoms; //FIXME
		reads += 1;
		double xyz_array[3];
		get_xyz(data, i, current_time, xyz_array);
		x = xyz_array[0];
		y = xyz_array[1];
		z = xyz_array[2];

		if (ts != NULL ) {
			ts->coords[3 * i] = x;
			ts->coords[3 * i + 1] = y;
			ts->coords[3 * i + 2] = z;
		} else {
			break;
		}
	}
	return MOLFILE_SUCCESS;
}

static void close_h5md_read(void *mydata) {
	 h5mddata *data = (h5mddata *)mydata;
	 H5Fclose(data->dataset_id);
	 free(data);
}

/* registration stuff */
static molfile_plugin_t plugin;

VMDPLUGIN_API int VMDPLUGIN_init() {
	memset(&plugin, 0, sizeof(molfile_plugin_t));
	plugin.abiversion = vmdplugin_ABIVERSION;
	plugin.type = MOLFILE_PLUGIN_TYPE;
	plugin.name = "h5md";
	plugin.prettyname = "h5md";
	plugin.author = "Sascha Ehrhardt, Jonas Landsgesell";
	plugin.majorv = 1;
	plugin.minorv = 3;
	plugin.is_reentrant = VMDPLUGIN_THREADSAFE;
	plugin.filename_extension = "h5";
	plugin.open_file_read = open_h5md_read;
	plugin.read_structure = read_h5md_structure;
	plugin.read_next_timestep = read_h5md_timestep;
	plugin.close_file_read = close_h5md_read;
	return VMDPLUGIN_SUCCESS;
}

VMDPLUGIN_API int VMDPLUGIN_register(void *v, vmdplugin_register_cb cb) {
	(*cb)(v, (vmdplugin_t *) &plugin);
	return VMDPLUGIN_SUCCESS;
}

VMDPLUGIN_API int VMDPLUGIN_fini() {
	return VMDPLUGIN_SUCCESS;
}
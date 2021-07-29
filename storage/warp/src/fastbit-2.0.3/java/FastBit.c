#include "FastBit.h"
#include "capi.h"
#include <stdlib.h>	// malloc, free

/**
   @File FastBit.c  Implements the Java Native Interface for FastBit.  This
   implementation is based on the C API defined in capi.h.
*/
/*
 * modified by li.zhengbing, 2010-05-13 In JNI, if using
 * GetStringUTFChars/GetXXXArrayElements, you must use
 * ReleaseStringUTFChars/ReleaseXXXArrayElements to release resource.
 * This modification adds ReleaseStringUTFChars/ReleaseXXXArrayElements. 
 */
JNIEXPORT void JNICALL Java_gov_lbl_fastbit_FastBit_init
(JNIEnv * env, jobject jo, jstring jrcfile) {
    jboolean        iscopy;
    const char     *crcfile;
    if (jrcfile != NULL)
	crcfile = (*env)->GetStringUTFChars(env, jrcfile, &iscopy);
    else
	crcfile = NULL;
    fastbit_init(crcfile);
    (*env)->ReleaseStringUTFChars(env, jrcfile, crcfile);
} /* Java_gov_lbl_fastbit_FastBit_init */

JNIEXPORT void JNICALL Java_gov_lbl_fastbit_FastBit_cleanup
(JNIEnv * env, jobject jo) {
    fastbit_cleanup();
} /* Java_gov_lbl_fastbit_FastBit_cleanup */

JNIEXPORT jint JNICALL Java_gov_lbl_fastbit_FastBit_number_1of_1rows
(JNIEnv * env, jobject jo, jstring jdir) {
    jint            ierr;
    jboolean        iscopy;
    const char     *cdir;
    if (jdir == 0) {
	ierr = -1;
	return ierr;
    }

    cdir = (*env)->GetStringUTFChars(env, jdir, &iscopy);
    ierr = fastbit_rows_in_partition(cdir);
    (*env)->ReleaseStringUTFChars(env, jdir, cdir);
    return ierr;
} /* Java_gov_lbl_fastbit_FastBit_number_1of_1rows */

JNIEXPORT jint JNICALL Java_gov_lbl_fastbit_FastBit_number_1of_1columns
(JNIEnv * env, jobject jo, jstring jdir) {
    jint            ierr;
    jboolean        iscopy;
    const char     *cdir;
    if (jdir == 0) {
	ierr = -1;
	return ierr;
    }

    cdir = (*env)->GetStringUTFChars(env, jdir, &iscopy);
    ierr = fastbit_columns_in_partition(cdir);
    /* add by lzb */
    (*env)->ReleaseStringUTFChars(env, jdir, cdir);
    return ierr;
} /* Java_gov_lbl_fastbit_FastBit_number_1of_1columns */

JNIEXPORT jint JNICALL Java_gov_lbl_fastbit_FastBit_build_1indexes
(JNIEnv * env, jobject jo, jstring jdir, jstring jopt) {
    jint            ierr;
    jboolean        iscopy;
    const char     *cdir;
    const char     *copt;
    if (jdir == NULL) {
	ierr = -1;
	return ierr;
    }

    cdir = (*env)->GetStringUTFChars(env, jdir, &iscopy);
    if (jopt != NULL)
	copt = (*env)->GetStringUTFChars(env, jopt, &iscopy);
    else
	copt = NULL;
    ierr = fastbit_build_indexes(cdir, copt);

    if (NULL != copt) {
	(*env)->ReleaseStringUTFChars(env, jopt, copt);
    }

    (*env)->ReleaseStringUTFChars(env, jdir, cdir);
    return ierr;
} /* Java_gov_lbl_fastbit_FastBit_build_1indexes */

JNIEXPORT jint JNICALL Java_gov_lbl_fastbit_FastBit_purge_1indexes
(JNIEnv * env, jobject jo, jstring jdir) {
    jint            ierr;
    jboolean        iscopy;
    const char     *cdir;
    if (jdir == NULL) {
	ierr = -1;
	return ierr;
    }

    cdir = (*env)->GetStringUTFChars(env, jdir, &iscopy);
    ierr = fastbit_purge_indexes(cdir);
    (*env)->ReleaseStringUTFChars(env, jdir, cdir);
    return ierr;
} /* Java_gov_lbl_fastbit_FastBit_purge_1indexes */

JNIEXPORT jint JNICALL Java_gov_lbl_fastbit_FastBit_build_1index
(JNIEnv * env, jobject jo, jstring jdir, jstring jcol, jstring jopt) {
    jint            ierr;
    jboolean        iscopy;
    const char     *cdir;
    const char     *ccol;
    const char     *copt;
    if (jdir == NULL || jcol == NULL) {
	ierr = -1;
	return ierr;
    }

    cdir = (*env)->GetStringUTFChars(env, jdir, &iscopy);
    ccol = (*env)->GetStringUTFChars(env, jcol, &iscopy);
    if (jopt != NULL)
	copt = (*env)->GetStringUTFChars(env, jopt, &iscopy);
    else
	copt = NULL;

    ierr = fastbit_build_index(cdir, ccol, copt);

    if (NULL != copt) {
	(*env)->GetStringUTFChars(env, jopt, &iscopy);
    }
    (*env)->ReleaseStringUTFChars(env, jcol, ccol);
    (*env)->ReleaseStringUTFChars(env, jdir, cdir);
    return ierr;
} /* Java_gov_lbl_fastbit_FastBit_build_1index */

JNIEXPORT jint JNICALL Java_gov_lbl_fastbit_FastBit_purge_1index
(JNIEnv * env, jobject jo, jstring jdir, jstring jcol) {
    jint            ierr;
    jboolean        iscopy;
    const char     *cdir;
    const char     *ccol;
    if (jdir == NULL) {
	ierr = -1;
	return ierr;
    }

    cdir = (*env)->GetStringUTFChars(env, jdir, &iscopy);
    if (jcol != NULL)
	ccol = (*env)->GetStringUTFChars(env, jcol, &iscopy);
    else
	ccol = NULL;

    ierr = fastbit_purge_index(cdir, ccol);

    if (NULL != ccol) {
	(*env)->ReleaseStringUTFChars(env, jcol, ccol);
    }
    (*env)->ReleaseStringUTFChars(env, jdir, cdir);
    return ierr;
} /* Java_gov_lbl_fastbit_FastBit_purge_1index */

JNIEXPORT jobject JNICALL Java_gov_lbl_fastbit_FastBit_build_1query
(JNIEnv * env, jobject jo, jstring jsel, jstring jdir, jstring jwhere) {
    jboolean           iscopy;
    const char        *csel;
    const char        *cdir;
    const char        *cwhere;
    FastBitQueryHandle handle;
    if (jdir == NULL || jwhere == NULL)
	return (jobject) 0;

    if (jsel != NULL)
	csel = (*env)->GetStringUTFChars(env, jsel, &iscopy);
    else
	csel = NULL;
    cdir = (*env)->GetStringUTFChars(env, jdir, &iscopy);
    cwhere = (*env)->GetStringUTFChars(env, jwhere, &iscopy);
    handle = fastbit_build_query(csel, cdir, cwhere);
    if (handle != 0) {
	jobject jret =
	    (*env)->NewDirectByteBuffer(env, handle, sizeof(handle));

	/* add by lzb */
	(*env)->ReleaseStringUTFChars(env, jsel, csel);
	(*env)->ReleaseStringUTFChars(env, jwhere, cwhere);
	(*env)->ReleaseStringUTFChars(env, jdir, cdir);
	return jret;
    } else {
	/* add by lzb */
	(*env)->ReleaseStringUTFChars(env, jsel, csel);
	(*env)->ReleaseStringUTFChars(env, jwhere, cwhere);
	(*env)->ReleaseStringUTFChars(env, jdir, cdir);
	return (jobject) (0);
    }
} /* Java_gov_lbl_fastbit_FastBit_build_1query */

JNIEXPORT jint JNICALL Java_gov_lbl_fastbit_FastBit_destroy_1query
(JNIEnv * env, jobject jo, jobject jhandle) {
    FastBitQueryHandle chandle = (FastBitQueryHandle)
	((*env)->GetDirectBufferAddress(env, jhandle));
    jint ierr = fastbit_destroy_query(chandle);
    return ierr;
} /* Java_gov_lbl_fastbit_FastBit_destroy_1query */

JNIEXPORT jintArray JNICALL Java_gov_lbl_fastbit_FastBit_get_1result_1row_1ids
(JNIEnv * env, jobject jo, jobject jhandle) {
    FastBitQueryHandle chandle = (FastBitQueryHandle)
	((*env)->GetDirectBufferAddress(env, jhandle));
    jintArray ret;
    jint ierr, nrows = fastbit_get_result_rows(chandle);
    if (nrows < 0) {
        return (jintArray) NULL;
    }
    ret = (*env)->NewIntArray(env, nrows);
    if (nrows == 0 || ret == NULL)
        return ret;
    uint32_t *tmp = (uint32_t*)malloc(sizeof(uint32_t)*nrows);
    if (tmp == 0)
        return (jintArray)NULL;
    ierr = fastbit_get_result_row_ids(chandle, tmp);
    if (ierr < 0) {
        free(tmp);
        return (jintArray) NULL;
    }
    else {
        (*env)->SetIntArrayRegion(env, ret, 0, nrows, (int*)tmp);
        free(tmp);
        return ret;
    }
} /* Java_gov_lbl_fastbit_FastBit_get_1result_1size */

JNIEXPORT jint JNICALL Java_gov_lbl_fastbit_FastBit_get_1result_1size
(JNIEnv * env, jobject jo, jobject jhandle) {
    FastBitQueryHandle chandle = (FastBitQueryHandle)
	((*env)->GetDirectBufferAddress(env, jhandle));
    jint ierr = fastbit_get_result_rows(chandle);
    return ierr;
} /* Java_gov_lbl_fastbit_FastBit_get_1result_1size */

JNIEXPORT jbyteArray JNICALL
Java_gov_lbl_fastbit_FastBit_get_1qualified_1bytes
(JNIEnv * env, jobject jo, jobject jhandle, jstring jcol) {
    jboolean           iscopy;
    const char        *ccol;
    jbyteArray         ret;
    const signed char *carr;
    jint               nrows;
    FastBitQueryHandle chandle = (FastBitQueryHandle)
	((*env)->GetDirectBufferAddress(env, jhandle));
    if (jhandle == NULL || jcol == NULL)
	return (jbyteArray) NULL;

    nrows = fastbit_get_result_rows(chandle);
    if (nrows <= 0) {
	ret = (*env)->NewByteArray(env, 0);
	return ret;
    }

    ccol = (*env)->GetStringUTFChars(env, jcol, &iscopy);
    carr = fastbit_get_qualified_bytes(chandle, ccol);
    if (carr != 0) {
	ret = (*env)->NewByteArray(env, nrows);
	if (ret != NULL) {
	    (*env)->SetByteArrayRegion(env, ret, 0, nrows,
				       (const signed char *) carr);
	}
    } else {
	ret = NULL;
    }
    (*env)->ReleaseStringUTFChars(env, jcol, ccol);
    return ret;
} /* Java_gov_lbl_fastbit_FastBit_get_1qualified_1bytes */

JNIEXPORT jshortArray JNICALL
Java_gov_lbl_fastbit_FastBit_get_1qualified_1shorts
(JNIEnv * env, jobject jo, jobject jhandle, jstring jcol) {
    jboolean        iscopy;
    const char     *ccol;
    jshortArray     ret;
    const int16_t  *carr;
    jint            nrows;
    FastBitQueryHandle chandle = (FastBitQueryHandle)
	((*env)->GetDirectBufferAddress(env, jhandle));
    if (jhandle == NULL || jcol == NULL)
	return (jshortArray) NULL;

    nrows = fastbit_get_result_rows(chandle);
    if (nrows <= 0) {
	ret = (*env)->NewShortArray(env, 0);
	return ret;
    }

    ccol = (*env)->GetStringUTFChars(env, jcol, &iscopy);
    carr = fastbit_get_qualified_shorts(chandle, ccol);
    if (carr != 0) {
	ret = (*env)->NewShortArray(env, nrows);
	if (ret != NULL) {
	    (*env)->SetShortArrayRegion(env, ret, 0, nrows, carr);
	}
    } else {
	ret = NULL;
    }
    (*env)->ReleaseStringUTFChars(env, jcol, ccol);
    return ret;
} /* Java_gov_lbl_fastbit_FastBit_get_1qualified_1shorts */

JNIEXPORT jintArray JNICALL
Java_gov_lbl_fastbit_FastBit_get_1qualified_1ints
(JNIEnv * env, jobject jo, jobject jhandle, jstring jcol) {
    jboolean        iscopy;
    const char     *ccol;
    jintArray       ret;
    const int32_t  *carr;
    jint            nrows;
    FastBitQueryHandle chandle = (FastBitQueryHandle)
	((*env)->GetDirectBufferAddress(env, jhandle));
    if (jhandle == NULL || jcol == NULL)
	return (jintArray) NULL;

    nrows = fastbit_get_result_rows(chandle);
    if (nrows <= 0) {
	ret = (*env)->NewIntArray(env, 0);
	return ret;
    }

    ccol = (*env)->GetStringUTFChars(env, jcol, &iscopy);
    carr = fastbit_get_qualified_ints(chandle, ccol);
    if (carr != 0) {
	ret = (*env)->NewIntArray(env, nrows);
	if (ret != NULL) {
	    (*env)->SetIntArrayRegion(env, ret, 0, nrows, carr);
	}
    } else {
	ret = NULL;
    }

    (*env)->ReleaseStringUTFChars(env, jcol, ccol);
    return ret;
} /* Java_gov_lbl_fastbit_FastBit_get_1qualified_1ints */

JNIEXPORT jlongArray JNICALL
Java_gov_lbl_fastbit_FastBit_get_1qualified_1longs
(JNIEnv * env, jobject jo, jobject jhandle, jstring jcol) {
    jboolean        iscopy;
    const char     *ccol;
    jlongArray      ret;
    const jlong    *carr;
    jint            nrows;
    FastBitQueryHandle chandle = (FastBitQueryHandle)
	((*env)->GetDirectBufferAddress(env, jhandle));
    if (jhandle == NULL || jcol == NULL)
	return (jlongArray) NULL;

    nrows = fastbit_get_result_rows(chandle);
    if (nrows <= 0) {
	ret = (*env)->NewLongArray(env, 0);
	return ret;
    }

    ccol = (*env)->GetStringUTFChars(env, jcol, &iscopy);
    carr = (jlong*)fastbit_get_qualified_longs(chandle, ccol);
    if (carr != 0) {
	ret = (*env)->NewLongArray(env, nrows);
	if (ret != NULL) {
	    (*env)->SetLongArrayRegion(env, ret, 0, nrows, carr);
	}
    } else {
	ret = NULL;
    }
    (*env)->ReleaseStringUTFChars(env, jcol, ccol);
    return ret;
} /* Java_gov_lbl_fastbit_FastBit_get_1qualified_1longs */

JNIEXPORT jfloatArray JNICALL
Java_gov_lbl_fastbit_FastBit_get_1qualified_1floats
(JNIEnv * env, jobject jo, jobject jhandle, jstring jcol) {
    jboolean        iscopy;
    const char     *ccol;
    jfloatArray     ret;
    const float    *carr;
    jint            nrows;
    FastBitQueryHandle chandle = (FastBitQueryHandle)
	((*env)->GetDirectBufferAddress(env, jhandle));
    if (jhandle == NULL || jcol == NULL)
	return (jfloatArray) NULL;

    nrows = fastbit_get_result_rows(chandle);
    if (nrows <= 0) {
	ret = (*env)->NewFloatArray(env, 0);
	return ret;
    }

    ccol = (*env)->GetStringUTFChars(env, jcol, &iscopy);
    carr = fastbit_get_qualified_floats(chandle, ccol);
    if (carr != 0) {
	ret = (*env)->NewFloatArray(env, nrows);
	if (ret != NULL) {
	    (*env)->SetFloatArrayRegion(env, ret, 0, nrows, carr);
	}
    } else {
	ret = NULL;
    }
    (*env)->ReleaseStringUTFChars(env, jcol, ccol);
    return ret;
} /* Java_gov_lbl_fastbit_FastBit_get_1qualified_1floats */

JNIEXPORT jdoubleArray JNICALL
Java_gov_lbl_fastbit_FastBit_get_1qualified_1doubles
(JNIEnv * env, jobject jo, jobject jhandle, jstring jcol) {
    jboolean        iscopy;
    const char     *ccol;
    jdoubleArray    ret;
    const double   *carr;
    jint            nrows;
    FastBitQueryHandle chandle = (FastBitQueryHandle)
	((*env)->GetDirectBufferAddress(env, jhandle));
    if (jhandle == NULL || jcol == NULL)
	return (jdoubleArray) NULL;

    nrows = fastbit_get_result_rows(chandle);
    if (nrows <= 0) {
	ret = (*env)->NewDoubleArray(env, 0);
	return ret;
    }

    ccol = (*env)->GetStringUTFChars(env, jcol, &iscopy);
    carr = fastbit_get_qualified_doubles(chandle, ccol);
    if (carr != 0) {
	ret = (*env)->NewDoubleArray(env, nrows);
	if (ret != NULL) {
	    (*env)->SetDoubleArrayRegion(env, ret, 0, nrows, carr);
	}
    } else {
	ret = NULL;
    }
    (*env)->ReleaseStringUTFChars(env, jcol, ccol);
    return ret;
} /* Java_gov_lbl_fastbit_FastBit_get_1qualified_1doubles */

JNIEXPORT jint JNICALL Java_gov_lbl_fastbit_FastBit_get_1message_1level
(JNIEnv * env, jobject jo) {
    return fastbit_get_verbose_level();
} /* Java_gov_lbl_fastbit_FastBit_get_1message_1level */

JNIEXPORT jint JNICALL Java_gov_lbl_fastbit_FastBit_set_1message_1level
(JNIEnv * env, jobject jo, jint jlvl) {
    return fastbit_set_verbose_level(jlvl);
} /* Java_gov_lbl_fastbit_FastBit_set_1message_1level */

JNIEXPORT jint JNICALL Java_gov_lbl_fastbit_FastBit_set_1logfile
(JNIEnv * env, jobject jo, jstring jfn) {
    jboolean    iscopy;
    const char *cfn;
    jint        ji;

    if (jfn != NULL)
	cfn = (*env)->GetStringUTFChars(env, jfn, &iscopy);
    else
	cfn = 0;

    ji = fastbit_set_logfile(cfn);
    (*env)->ReleaseStringUTFChars(env, jfn, cfn);
    return ji;
} /* Java_gov_lbl_fastbit_FastBit_set_1logfile */

JNIEXPORT jstring JNICALL Java_gov_lbl_fastbit_FastBit_get_1logfile
(JNIEnv * env, jobject jo) {
    return (*env)->NewStringUTF(env, fastbit_get_logfile());
} /* Java_gov_lbl_fastbit_FastBit_get_1logfile */

JNIEXPORT jint JNICALL Java_gov_lbl_fastbit_FastBit_write_1buffer
(JNIEnv * env, jobject jo, jstring jdir) {
    jint        ierr;
    jboolean    iscopy;
    const char *cdir;
    if (jdir == NULL) {
	ierr = -1;
	return ierr;
    }

    cdir = (*env)->GetStringUTFChars(env, jdir, &iscopy);
    ierr = fastbit_flush_buffer(cdir);
    (*env)->ReleaseStringUTFChars(env, jdir, cdir);
    return ierr;
} /* Java_gov_lbl_fastbit_FastBit_write_1buffer */

JNIEXPORT jint JNICALL Java_gov_lbl_fastbit_FastBit_add_1doubles
(JNIEnv * env, jobject jo, jstring jcol, jdoubleArray jvals) {
    jint        ierr = -1;
    jboolean    iscopy;
    jsize       nelm;
    jdouble    *dblArr;
    const char *ccol;
    const char *type = "double";
    if (jcol == NULL || jvals == NULL) {
	return ierr;
    }
    /*
     * ccol = (*env)->GetStringUTFChars(env, jcol, &iscopy); nelm =
     * (*env)->GetArrayLength(env, jvals); ierr = fastbit_add_values
     * (ccol, type, (*env)->GetDoubleArrayElements(env, jvals, &iscopy),
     * nelm, 0U);
     */
    /* modified by lzb */
    ccol = (*env)->GetStringUTFChars(env, jcol, &iscopy);
    nelm = (*env)->GetArrayLength(env, jvals);
    dblArr = (*env)->GetDoubleArrayElements(env, jvals, &iscopy);
    ierr = fastbit_add_values(ccol, type, dblArr, nelm, 0U);
    (*env)->ReleaseDoubleArrayElements(env, jvals, dblArr, 0);
    (*env)->ReleaseStringUTFChars(env, jcol, ccol);
    return ierr;
} /* Java_gov_lbl_fastbit_FastBit_add_1doubles */

JNIEXPORT jint JNICALL Java_gov_lbl_fastbit_FastBit_add_1floats
(JNIEnv * env, jobject jo, jstring jcol, jfloatArray jvals) {
    jint        ierr = -1;
    jboolean    iscopy;
    jsize       nelm;
    jfloat     *fltArr;
    const char *ccol;
    const char *type = "float";
    if (jcol == NULL || jvals == NULL) {
	return ierr;
    }
    ccol = (*env)->GetStringUTFChars(env, jcol, &iscopy);
    nelm = (*env)->GetArrayLength(env, jvals);

    fltArr = (*env)->GetFloatArrayElements(env, jvals, &iscopy);
    ierr = fastbit_add_values(ccol, type, fltArr, nelm, 0U);

    (*env)->ReleaseFloatArrayElements(env, jvals, fltArr, 0);
    (*env)->ReleaseStringUTFChars(env, jcol, ccol);
    return ierr;
} /* Java_gov_lbl_fastbit_FastBit_add_1floats */

JNIEXPORT jint JNICALL Java_gov_lbl_fastbit_FastBit_add_1longs
(JNIEnv * env, jobject jo, jstring jcol, jlongArray jvals) {
    jint        ierr = -1;
    jboolean    iscopy;
    jsize       nelm;
    jlong      *lArr;
    const char *ccol;
    const char *type = "long";
    if (jcol == NULL || jvals == NULL) {
	return ierr;
    }
    ccol = (*env)->GetStringUTFChars(env, jcol, &iscopy);
    nelm = (*env)->GetArrayLength(env, jvals);

    lArr = (*env)->GetLongArrayElements(env, jvals, &iscopy);
    ierr = fastbit_add_values(ccol, type, lArr, nelm, 0U);

    (*env)->ReleaseLongArrayElements(env, jvals, lArr, 0);
    (*env)->ReleaseStringUTFChars(env, jcol, ccol);
    return ierr;
} /* Java_gov_lbl_fastbit_FastBit_add_1longs */

JNIEXPORT jint JNICALL Java_gov_lbl_fastbit_FastBit_add_1ints
(JNIEnv * env, jobject jo, jstring jcol, jintArray jvals) {
    jint        ierr = -1;
    jboolean    iscopy;
    jsize       nelm;
    jint       *iArr;
    const char *ccol;
    const char *type = "int";
    if (jcol == NULL || jvals == NULL) {
	return ierr;
    }
    ccol = (*env)->GetStringUTFChars(env, jcol, &iscopy);
    nelm = (*env)->GetArrayLength(env, jvals);

    iArr = (*env)->GetIntArrayElements(env, jvals, &iscopy);
    ierr = fastbit_add_values(ccol, type, iArr, nelm, 0U);

    (*env)->ReleaseIntArrayElements(env, jvals, iArr, 0);
    (*env)->ReleaseStringUTFChars(env, jcol, ccol);
    return ierr;
} /* Java_gov_lbl_fastbit_FastBit_add_1ints */

JNIEXPORT jint JNICALL Java_gov_lbl_fastbit_FastBit_add_1shorts
(JNIEnv * env, jobject jo, jstring jcol, jshortArray jvals) {
    jint        ierr = -1;
    jboolean    iscopy;
    jsize       nelm;
    jshort     *shtArr;
    const char *ccol;
    const char *type = "short";
    if (jcol == NULL || jvals == NULL) {
	return ierr;
    }
    ccol = (*env)->GetStringUTFChars(env, jcol, &iscopy);
    nelm = (*env)->GetArrayLength(env, jvals);

    shtArr = (*env)->GetShortArrayElements(env, jvals, &iscopy);
    ierr = fastbit_add_values(ccol, type, shtArr, nelm, 0U);

    (*env)->ReleaseShortArrayElements(env, jvals, shtArr, 0);
    (*env)->ReleaseStringUTFChars(env, jcol, ccol);
    return ierr;
} /* Java_gov_lbl_fastbit_FastBit_add_1shorts */

JNIEXPORT jint JNICALL Java_gov_lbl_fastbit_FastBit_add_1bytes
(JNIEnv * env, jobject jo, jstring jcol, jbyteArray jvals) {
    jint        ierr = -1;
    jboolean    iscopy;
    jsize       nelm;
    jbyte      *btArr;
    const char *ccol;
    const char *type = "byte";
    if (jcol == NULL || jvals == NULL) {
	return ierr;
    }
    ccol = (*env)->GetStringUTFChars(env, jcol, &iscopy);
    nelm = (*env)->GetArrayLength(env, jvals);
    btArr = (*env)->GetByteArrayElements(env, jvals, &iscopy);
    ierr = fastbit_add_values(ccol, type, btArr, nelm, 0U);

    (*env)->ReleaseByteArrayElements(env, jvals, btArr, 0);
    (*env)->ReleaseStringUTFChars(env, jcol, ccol);
    return ierr;
} /* Java_gov_lbl_fastbit_FastBit_add_1bytes */

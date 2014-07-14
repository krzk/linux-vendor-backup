/*
 * Samsung EXYNOS5 FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
 * Kil-yeon Lim <kilyeon.im@samsung.com>
 * Arun Kumar K <arun.kk@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_METADATA_H_
#define FIMC_IS_METADATA_H_

struct rational {
	uint32_t num;
	uint32_t den;
};

/**
 * @brief Metadata type: basic/ extended
 */
#define CAMERA2_SHOT_BASE_MODE	1
#define CAMERA2_SHOT_EXT_MODE	2

#define CAMERA2_MAX_AVAILABLE_MODE	21
#define CAMERA2_MAX_FACES		16

#define CAMERA2_MAX_VENDER_LENGTH		400
#define CAPTURE_NODE_MAX			2
#define CAMERA2_MAX_PDAF_MULTIROI_COLUMN	9
#define CAMERA2_MAX_PDAF_MULTIROI_ROW		5


/*
 * Controls/dynamic metadata
 */

enum metadata_mode {
	METADATA_MODE_NONE,
	METADATA_MODE_FULL
};

struct camera2_request_ctl{
        struct camera2_request_ctl_base {
                uint32_t		id;
                enum metadata_mode	metadatamode;
                uint8_t			outputstreams[16];
                uint32_t		framecount;
        }base;
        uint32_t requestCount; /* metadata v2 */
};

struct camera2_request_dm {
        struct camera2_request_dm_base {
                uint32_t		id;
                enum metadata_mode	metadatamode;
                uint32_t		framecount;
        }base;
        uint32_t requestcount; /* metadata v2 */
};

/**
 * struct camera2_entry_ctl	per-frame control for entry control
 * @lowIndexParam:		parameters flag - low 32-bits
 * @highIndexParam:		parameters flag - high 32-bits
 * @parameter			set of parameters
 */
struct camera2_entry_ctl {
        uint32_t		lowIndexParam;
        uint32_t		highIndexParam;
        uint32_t		parameter[2048];
};

struct camera2_entry_dm {
        uint32_t		lowIndexParam;
        uint32_t		highIndexParam;
};


enum optical_stabilization_mode {
	OPTICAL_STABILIZATION_MODE_OFF,
	OPTICAL_STABILIZATION_MODE_ON
};

enum lens_facing {
	LENS_FACING_BACK,
	LENS_FACING_FRONT
};

struct camera2_lens_ctl {
	uint32_t				focus_distance;
	float					aperture;
	float					focal_length;
	float					filter_density;
	enum optical_stabilization_mode		optical_stabilization_mode;
};

struct camera2_lens_dm {
	uint32_t				focus_distance;
	float					aperture;
	float					focal_length;
	float					filter_density;
	enum optical_stabilization_mode		optical_stabilization_mode;
	float					focus_range[2];
};

struct camera2_lens_sm {
	float				minimum_focus_distance;
	float				hyper_focal_distance;
	float				available_focal_length[2];
	float				available_apertures;
	/* assuming 1 aperture */
	float				available_filter_densities;
	/* assuming 1 ND filter value */
	enum optical_stabilization_mode	available_optical_stabilization;
	/* assuming 1 */
	uint32_t			shading_map_size;
	float				shading_map[3][40][30];
	uint32_t			geometric_correction_map_size;
	float				geometric_correction_map[2][3][40][30];
	enum lens_facing		facing;
	float				position[2];
};

enum sensor_colorfilter_arrangement {
	SENSOR_COLORFILTER_ARRANGEMENT_RGGB,
	SENSOR_COLORFILTER_ARRANGEMENT_GRBG,
	SENSOR_COLORFILTER_ARRANGEMENT_GBRG,
	SENSOR_COLORFILTER_ARRANGEMENT_BGGR,
	SENSOR_COLORFILTER_ARRANGEMENT_RGB
};

enum sensor_ref_illuminant {
	SENSOR_ILLUMINANT_DAYLIGHT = 1,
	SENSOR_ILLUMINANT_FLUORESCENT = 2,
	SENSOR_ILLUMINANT_TUNGSTEN = 3,
	SENSOR_ILLUMINANT_FLASH = 4,
	SENSOR_ILLUMINANT_FINE_WEATHER = 9,
	SENSOR_ILLUMINANT_CLOUDY_WEATHER = 10,
	SENSOR_ILLUMINANT_SHADE = 11,
	SENSOR_ILLUMINANT_DAYLIGHT_FLUORESCENT = 12,
	SENSOR_ILLUMINANT_DAY_WHITE_FLUORESCENT = 13,
	SENSOR_ILLUMINANT_COOL_WHITE_FLUORESCENT = 14,
	SENSOR_ILLUMINANT_WHITE_FLUORESCENT = 15,
	SENSOR_ILLUMINANT_STANDARD_A = 17,
	SENSOR_ILLUMINANT_STANDARD_B = 18,
	SENSOR_ILLUMINANT_STANDARD_C = 19,
	SENSOR_ILLUMINANT_D55 = 20,
	SENSOR_ILLUMINANT_D65 = 21,
	SENSOR_ILLUMINANT_D75 = 22,
	SENSOR_ILLUMINANT_D50 = 23,
	SENSOR_ILLUMINANT_ISO_STUDIO_TUNGSTEN = 24
};

struct camera2_sensor_ctl {
	/* unit : nano */
	uint64_t	exposure_time;
	/* unit : nano(It's min frame duration */
	uint64_t	frame_duration;
	/* unit : percent(need to change ISO value?) */
	uint32_t	sensitivity;
};

struct camera2_sensor_dm {
        struct camera2_sensor_dm_base {
                uint64_t		exposure_time;
                uint64_t		frame_duration;
                uint32_t		sensitivity;
                uint64_t		timestamp;
        }base;
        uint32_t analog_gain;  /* metadata v2 */
        uint32_t digital_gain; /* metadata v2 */
};

struct camera2_sensor_sm {
	uint32_t	exposure_time_range[2];
	uint32_t	max_frame_duration;
	/* list of available sensitivities. */
	uint32_t	available_sensitivities[10];
	enum sensor_colorfilter_arrangement colorfilter_arrangement;
	float		physical_size[2];
	uint32_t	pixel_array_size[2];
	uint32_t	active_array_size[4];
	uint32_t	white_level;
	uint32_t	black_level_pattern[4];
	struct rational	color_transform1[9];
	struct rational	color_transform2[9];
	enum sensor_ref_illuminant	reference_illuminant1;
	enum sensor_ref_illuminant	reference_illuminant2;
	struct rational	forward_matrix1[9];
	struct rational	forward_matrix2[9];
	struct rational	calibration_transform1[9];
	struct rational	calibration_transform2[9];
	struct rational	base_gain_factor;
	uint32_t	max_analog_sensitivity;
	float		noise_model_coefficients[2];
	uint32_t	orientation;
};



enum flash_mode {
	CAM2_FLASH_MODE_OFF = 1,
	CAM2_FLASH_MODE_SINGLE,
	CAM2_FLASH_MODE_TORCH,
	CAM2_FLASH_MODE_BEST
};

struct camera2_flash_ctl {
	enum flash_mode		flash_mode;
	uint32_t		firing_power;
	uint64_t		firing_time;
};

struct camera2_flash_dm {
        struct camera2_flash_dm_base {
                enum flash_mode		flash_mode;
                /* 10 is max power */
                uint32_t		firing_power;
                /* unit : microseconds */
                uint64_t		firing_time;
                /* 1 : stable, 0 : unstable */
                uint32_t		firing_stable;
                /* 1 : success, 0 : fail */
                uint32_t		decision;
        }base;
        /* 0: None, 1 : pre, 2 : main flash ready */
        uint32_t flashReady;    /* metadata v2 */

        /* 0: None, 1 : pre, 2 : main flash off ready */
        uint32_t flashOffReady; /* metadata v2 */
};

struct camera2_flash_sm {
	uint32_t	available;
	uint64_t	charge_duration;
};

enum processing_mode {
	PROCESSING_MODE_OFF = 1,
	PROCESSING_MODE_FAST,
	PROCESSING_MODE_HIGH_QUALITY
};


struct camera2_hotpixel_ctl {
	enum processing_mode	mode;
};

struct camera2_hotpixel_dm {
	enum processing_mode	mode;
};

struct camera2_demosaic_ctl {
	enum processing_mode	mode;
};

struct camera2_demosaic_dm {
	enum processing_mode	mode;
};

struct camera2_noise_reduction_ctl {
	enum processing_mode	mode;
	uint32_t		strength;
};

struct camera2_noise_reduction_dm {
	enum processing_mode	mode;
	uint32_t		strength;
};

struct camera2_shading_ctl {
	enum processing_mode	mode;
};

struct camera2_shading_dm {
	enum processing_mode	mode;
};

struct camera2_geometric_ctl {
	enum processing_mode	mode;
};

struct camera2_geometric_dm {
	enum processing_mode	mode;
};

enum color_correction_mode {
	COLOR_CORRECTION_MODE_FAST = 1,
	COLOR_CORRECTION_MODE_HIGH_QUALITY,
	COLOR_CORRECTION_MODE_TRANSFORM_MATRIX,
	COLOR_CORRECTION_MODE_EFFECT_MONO,
	COLOR_CORRECTION_MODE_EFFECT_NEGATIVE,
	COLOR_CORRECTION_MODE_EFFECT_SOLARIZE,
	COLOR_CORRECTION_MODE_EFFECT_SEPIA,
	COLOR_CORRECTION_MODE_EFFECT_POSTERIZE,
	COLOR_CORRECTION_MODE_EFFECT_WHITEBOARD,
	COLOR_CORRECTION_MODE_EFFECT_BLACKBOARD,
        COLOR_CORRECTION_MODE_EFFECT_AQUA,

        COLORCORRECTION_MODE_EFFECT_EMBOSS,
        COLORCORRECTION_MODE_EFFECT_EMBOSS_MONO,
        COLORCORRECTION_MODE_EFFECT_SKETCH,
        COLORCORRECTION_MODE_EFFECT_RED_YELLOW_POINT,
        COLORCORRECTION_MODE_EFFECT_GREEN_POINT,
        COLORCORRECTION_MODE_EFFECT_BLUE_POINT,
        COLORCORRECTION_MODE_EFFECT_MAGENTA_POINT,
        COLORCORRECTION_MODE_EFFECT_WARM_VINTAGE,
        COLORCORRECTION_MODE_EFFECT_COLD_VINTAGE,
        COLORCORRECTION_MODE_EFFECT_WASHED,
        TOTAOCOUNT_COLORCORRECTION_MODE_EFFECT
};

struct camera2_color_correction_ctl {
        struct camera2_color_correction_ctl_base {
                enum color_correction_mode	mode;
                float				transform[9];
                uint32_t			hue;
                uint32_t			saturation;
                uint32_t			brightness;
        }base;
        uint32_t contrast; /* metadata v2 */
};

struct camera2_color_correction_dm {
        struct camera2_color_correction_dm_base {
                enum color_correction_mode	mode;
                float				transform[9];
                uint32_t			hue;
                uint32_t			saturation;
                uint32_t			brightness;
        }base;
        uint32_t contrast; /* metadata v2 */
};

struct camera2_color_correction_sm {
        struct camera2_color_correction_sm_base {
                /* assuming 10 supported modes */
                uint8_t			available_modes[CAMERA2_MAX_AVAILABLE_MODE];
                uint32_t		hue_range[2];
                uint32_t		saturation_range[2];
                uint32_t		brightness_range[2];
        }base;
        uint32_t contrastRange[2]; /* metadata v2 */
};

enum tonemap_mode {
	TONEMAP_MODE_FAST = 1,
	TONEMAP_MODE_HIGH_QUALITY,
	TONEMAP_MODE_CONTRAST_CURVE
};

struct camera2_tonemap_ctl {
	enum tonemap_mode		mode;
	/* assuming maxCurvePoints = 64 */
	float				curve_red[64];
	float				curve_green[64];
	float				curve_blue[64];
};

struct camera2_tonemap_dm {
	enum tonemap_mode		mode;
	/* assuming maxCurvePoints = 64 */
	float				curve_red[64];
	float				curve_green[64];
	float				curve_blue[64];
};

struct camera2_tonemap_sm {
	uint32_t	max_curve_points;
};

struct camera2_edge_ctl {
	enum processing_mode	mode;
	uint32_t		strength;
};

struct camera2_edge_dm {
	enum processing_mode	mode;
	uint32_t		strength;
};

enum scaler_formats {
	SCALER_FORMAT_BAYER_RAW,
	SCALER_FORMAT_YV12,
	SCALER_FORMAT_NV21,
	SCALER_FORMAT_JPEG,
	SCALER_FORMAT_UNKNOWN
};

struct camera2_scaler_ctl {
        struct camera2_scaler_ctl_base {
                uint32_t		crop_region[4];
        }base;
        uint32_t orientation; /* metadata v2 */
};

struct camera2_scaler_dm {
        struct camera2_scaler_dm_base {
                uint32_t		crop_region[4];
        }base;
        uint32_t orientation; /* metadata v2 */
};

struct camera2_scaler_sm {
	enum scaler_formats available_formats[4];
	/* assuming # of availableFormats = 4 */
	uint32_t	available_raw_sizes;
	uint64_t	available_raw_min_durations;
	/* needs check */
	uint32_t	available_processed_sizes[8];
	uint64_t	available_processed_min_durations[8];
	uint32_t	available_jpeg_sizes[8][2];
	uint64_t	available_jpeg_min_durations[8];
	uint32_t	available_max_digital_zoom[8];
};

struct camera2_jpeg_ctl {
	uint32_t	quality;
	uint32_t	thumbnail_size[2];
	uint32_t	thumbnail_quality;
	double		gps_coordinates[3];
	uint32_t	gps_processing_method;
	uint64_t	gps_timestamp;
	uint32_t	orientation;
};

struct camera2_jpeg_dm {
	uint32_t	quality;
	uint32_t	thumbnail_size[2];
	uint32_t	thumbnail_quality;
	double		gps_coordinates[3];
	uint32_t	gps_processing_method;
	uint64_t	gps_timestamp;
	uint32_t	orientation;
};

struct camera2_jpeg_sm {
	uint32_t	available_thumbnail_sizes[8][2];
	uint32_t	maxsize;
	/* assuming supported size=8 */
};

enum face_detect_mode {
	FACEDETECT_MODE_OFF = 1,
	FACEDETECT_MODE_SIMPLE,
	FACEDETECT_MODE_FULL
};

enum stats_mode {
	STATS_MODE_OFF = 1,
	STATS_MODE_ON
};

enum stats_lowlightmode {
        STATE_LLS_REQUIRED = 1
};

struct camera2_stats_ctl {
	enum face_detect_mode	face_detect_mode;
	enum stats_mode		histogram_mode;
	enum stats_mode		sharpness_map_mode;
};

struct camera2_stats_dm {
        struct camera2_stats_dm_base {
                enum face_detect_mode	face_detect_mode;
                uint32_t		face_rectangles[CAMERA2_MAX_FACES][4];
                uint8_t			face_scores[CAMERA2_MAX_FACES];
                uint32_t		face_landmarks[CAMERA2_MAX_FACES][6];
                uint32_t		face_ids[CAMERA2_MAX_FACES];
                enum stats_mode		histogram_mode;
                uint32_t		histogram[3 * 256];
                enum stats_mode		sharpness_map_mode;
        }base;
        /* sharpnessMap */
        enum stats_lowlightmode LowLightMode;         /* metadata v2 */
        uint32_t 		lls_tuning_set_index; /* metadata v2 */
        uint32_t 		lls_brightness_index; /* metadata v2 */
};


struct camera2_stats_sm {
	uint8_t		available_face_detect_modes[CAMERA2_MAX_AVAILABLE_MODE];
	/* assuming supported modes = 3 */
	uint32_t	max_face_count;
	uint32_t	histogram_bucket_count;
	uint32_t	max_histogram_count;
	uint32_t	sharpness_map_size[2];
	uint32_t	max_sharpness_map_value;
};

enum aa_capture_intent {
	AA_CAPTURE_INTENT_CUSTOM = 0,
	AA_CAPTURE_INTENT_PREVIEW,
	AA_CAPTURE_INTENT_STILL_CAPTURE,
	AA_CAPTURE_INTENT_VIDEO_RECORD,
	AA_CAPTURE_INTENT_VIDEO_SNAPSHOT,
	AA_CAPTURE_INTENT_ZERO_SHUTTER_LAG
};

enum aa_mode {
	AA_CONTROL_OFF = 1,
	AA_CONTROL_AUTO,
	AA_CONTROL_USE_SCENE_MODE
};

enum aa_scene_mode {
	AA_SCENE_MODE_UNSUPPORTED = 1,
	AA_SCENE_MODE_FACE_PRIORITY,
	AA_SCENE_MODE_ACTION,
	AA_SCENE_MODE_PORTRAIT,
	AA_SCENE_MODE_LANDSCAPE,
	AA_SCENE_MODE_NIGHT,
	AA_SCENE_MODE_NIGHT_PORTRAIT,
	AA_SCENE_MODE_THEATRE,
	AA_SCENE_MODE_BEACH,
	AA_SCENE_MODE_SNOW,
	AA_SCENE_MODE_SUNSET,
	AA_SCENE_MODE_STEADYPHOTO,
	AA_SCENE_MODE_FIREWORKS,
	AA_SCENE_MODE_SPORTS,
	AA_SCENE_MODE_PARTY,
	AA_SCENE_MODE_CANDLELIGHT,
	AA_SCENE_MODE_BARCODE,
        AA_SCENE_MODE_NIGHT_CAPTURE,

        AA_SCENE_MODE_ANTISHAKE,
        AA_SCENE_MODE_HDR,
        AA_SCENE_MODE_LLS,
        AA_SCENE_MODE_FDAE,
	AA_SCENE_MODE_DUAL,
        AA_SCENE_MODE_DRAMA,
        AA_SCENE_MODE_ANIMATED,
        AA_SCENE_MODE_PANORAMA,
        AA_SCENE_MODE_GOLF,
        AA_SCENE_MODE_PREVIEW,
        AA_SCENE_MODE_VIDEO,
        AA_SCENE_MODE_SLOWMOTION_2,
        AA_SCENE_MODE_SLOWMOTION_4_8,
        AA_SCENE_MODE_DUAL_PREVIEW,
        AA_SCENE_MODE_DUAL_VIDEO,
        AA_SCENE_MODE_120_PREVIEW,
        AA_SCENE_MODE_LIGHT_TRACE
};

enum aa_effect_mode {
	AA_EFFECT_OFF = 1,
	AA_EFFECT_MONO,
	AA_EFFECT_NEGATIVE,
	AA_EFFECT_SOLARIZE,
	AA_EFFECT_SEPIA,
	AA_EFFECT_POSTERIZE,
	AA_EFFECT_WHITEBOARD,
	AA_EFFECT_BLACKBOARD,
	AA_EFFECT_AQUA
};

enum aa_aemode {
	AA_AEMODE_OFF = 1,
	AA_AEMODE_LOCKED,
	AA_AEMODE_ON,
	AA_AEMODE_ON_AUTO_FLASH,
	AA_AEMODE_ON_ALWAYS_FLASH,
	AA_AEMODE_ON_AUTO_FLASH_REDEYE
};

enum aa_ae_flashmode {
	/* all flash control stop */
	AA_FLASHMODE_OFF = 1,
	/* internal 3A can control flash */
	AA_FLASHMODE_ON,
	/* internal 3A can do auto flash algorithm */
	AA_FLASHMODE_AUTO,
	/* internal 3A can fire flash by auto result */
	AA_FLASHMODE_CAPTURE,
	/* internal 3A can control flash forced */
	AA_FLASHMODE_ON_ALWAYS

};

enum aa_ae_antibanding_mode {
	AA_AE_ANTIBANDING_OFF = 1,
	AA_AE_ANTIBANDING_50HZ,
	AA_AE_ANTIBANDING_60HZ,
        AA_AE_ANTIBANDING_AUTO,
        AA_AE_ANTIBANDING_AUTO_50HZ,   /* 50Hz + Auto NEW */
        AA_AE_ANTIBANDING_AUTO_60HZ    /* 60Hz + Auto NEW */
};

enum aa_awbmode {
	AA_AWBMODE_OFF = 1,
	AA_AWBMODE_LOCKED,
	AA_AWBMODE_WB_AUTO,
	AA_AWBMODE_WB_INCANDESCENT,
	AA_AWBMODE_WB_FLUORESCENT,
	AA_AWBMODE_WB_WARM_FLUORESCENT,
	AA_AWBMODE_WB_DAYLIGHT,
	AA_AWBMODE_WB_CLOUDY_DAYLIGHT,
	AA_AWBMODE_WB_TWILIGHT,
	AA_AWBMODE_WB_SHADE
};

enum aa_afmode {
	AA_AFMODE_OFF = 1,
	AA_AFMODE_AUTO,
	AA_AFMODE_MACRO,
	AA_AFMODE_CONTINUOUS_VIDEO,
	AA_AFMODE_CONTINUOUS_PICTURE,
	AA_AFMODE_EDOF
};

enum aa_afstate {
	AA_AFSTATE_INACTIVE = 1,
	AA_AFSTATE_PASSIVE_SCAN,
	AA_AFSTATE_ACTIVE_SCAN,
	AA_AFSTATE_AF_ACQUIRED_FOCUS,
	AA_AFSTATE_AF_FAILED_FOCUS
};

enum ae_state {
	AE_STATE_INACTIVE = 1,
	AE_STATE_SEARCHING,
	AE_STATE_CONVERGED,
	AE_STATE_LOCKED,
	AE_STATE_FLASH_REQUIRED,
	AE_STATE_PRECAPTURE
};

enum awb_state {
	AWB_STATE_INACTIVE = 1,
	AWB_STATE_SEARCHING,
	AWB_STATE_CONVERGED,
	AWB_STATE_LOCKED
};

enum aa_isomode {
	AA_ISOMODE_AUTO = 1,
	AA_ISOMODE_MANUAL,
};

struct camera2_aa_ctl {
	enum aa_capture_intent		capture_intent;
	enum aa_mode			mode;
	enum aa_scene_mode		scene_mode;
	uint32_t			video_stabilization_mode;
	enum aa_aemode			ae_mode;
	uint32_t			ae_regions[5];
	/* 5 per region(x1,y1,x2,y2,weight). Currently assuming 1 region. */
	int32_t				ae_exp_compensation;
	uint32_t			ae_target_fps_range[2];
	enum aa_ae_antibanding_mode	ae_anti_banding_mode;
	enum aa_ae_flashmode		ae_flash_mode;
	enum aa_awbmode			awb_mode;
	uint32_t			awb_regions[5];
	/* 5 per region(x1,y1,x2,y2,weight). Currently assuming 1 region. */
	enum aa_afmode			af_mode;
	uint32_t			af_regions[5];
	/* 5 per region(x1,y1,x2,y2,weight). Currently assuming 1 region. */
	uint32_t			af_trigger;
	enum aa_isomode			iso_mode;
	uint32_t			iso_value;

};

struct camera2_aa_dm {
	enum aa_mode				mode;
	enum aa_effect_mode			effect_mode;
	enum aa_scene_mode			scene_mode;
	uint32_t				video_stabilization_mode;
	enum aa_aemode				ae_mode;
	uint32_t				ae_regions[5];
	/* 5 per region(x1,y1,x2,y2,weight). Currently assuming 1 region. */
	enum ae_state				ae_state;
	enum aa_ae_flashmode			ae_flash_mode;
	enum aa_awbmode				awb_mode;
	uint32_t				awb_regions[5];
	enum awb_state				awb_state;
	/* 5 per region(x1,y1,x2,y2,weight). Currently assuming 1 region. */
	enum aa_afmode				af_mode;
	uint32_t				af_regions[5];
	/* 5 per region(x1,y1,x2,y2,weight). Currently assuming 1 region */
	enum aa_afstate				af_state;
	enum aa_isomode				iso_mode;
	uint32_t				iso_value;
};

struct camera2_aa_sm {
        uint8_t		available_scene_modes[CAMERA2_MAX_AVAILABLE_MODE];
        uint8_t		available_effects[CAMERA2_MAX_AVAILABLE_MODE];
	/* Assuming # of available scene modes = 10 */
        uint32_t	max_regions;
        uint8_t		ae_available_modes[CAMERA2_MAX_AVAILABLE_MODE];
	/* Assuming # of available ae modes = 8 */
	struct rational	ae_compensation_step;
        int32_t		ae_compensation_range[2];
        uint32_t	ae_available_target_fps_ranges[CAMERA2_MAX_AVAILABLE_MODE][2];
        uint8_t		ae_available_antibanding_modes[CAMERA2_MAX_AVAILABLE_MODE];
        uint8_t		awb_available_modes[CAMERA2_MAX_AVAILABLE_MODE];
	/* Assuming # of awbAvailableModes = 10 */
        uint8_t		af_available_modes[CAMERA2_MAX_AVAILABLE_MODE];
	/* Assuming # of afAvailableModes = 4 */
        uint8_t		available_video_stabilization_modes[4];
	/* Assuming # of availableVideoStabilizationModes = 4 */
        uint32_t	iso_range[2];
};

struct camera2_lens_usm {
	/* Frame delay between sending command and applying frame data */
	uint32_t	focus_distance_frame_delay;
};

struct camera2_sensor_usm {
	/* Frame delay between sending command and applying frame data */
	uint32_t	exposure_time_frame_delay;
	uint32_t	frame_duration_frame_delay;
	uint32_t	sensitivity_frame_delay;
};

struct camera2_flash_usm {
	/* Frame delay between sending command and applying frame data */
	uint32_t	flash_mode_frame_delay;
	uint32_t	firing_power_frame_delay;
	uint64_t	firing_time_frame_delay;
};

struct camera2_ctl_base {
        struct camera2_request_ctl_base		request;
        struct camera2_lens_ctl			lens;
        struct camera2_sensor_ctl		sensor;
        struct camera2_flash_ctl		flash;
        struct camera2_hotpixel_ctl		hotpixel;
        struct camera2_demosaic_ctl		demosaic;
        struct camera2_noise_reduction_ctl	noise;
        struct camera2_shading_ctl		shading;
        struct camera2_geometric_ctl		geometric;
        struct camera2_color_correction_ctl_base color;
        struct camera2_tonemap_ctl		tonemap;
        struct camera2_edge_ctl			edge;
        struct camera2_scaler_ctl_base		scaler;
        struct camera2_jpeg_ctl			jpeg;
        struct camera2_stats_ctl		stats;
        struct camera2_aa_ctl			aa;
};

struct camera2_ctl {
	struct camera2_request_ctl		request;
	struct camera2_lens_ctl			lens;
	struct camera2_sensor_ctl		sensor;
	struct camera2_flash_ctl		flash;
	struct camera2_hotpixel_ctl		hotpixel;
	struct camera2_demosaic_ctl		demosaic;
	struct camera2_noise_reduction_ctl	noise;
	struct camera2_shading_ctl		shading;
	struct camera2_geometric_ctl		geometric;
	struct camera2_color_correction_ctl	color;
	struct camera2_tonemap_ctl		tonemap;
	struct camera2_edge_ctl			edge;
	struct camera2_scaler_ctl		scaler;
	struct camera2_jpeg_ctl			jpeg;
	struct camera2_stats_ctl		stats;
	struct camera2_aa_ctl			aa;
        struct camera2_entry_ctl 		entry; /* metadata v2 */
};

struct camera2_dm_base {
        struct camera2_request_dm_base		request;
        struct camera2_lens_dm			lens;
        struct camera2_sensor_dm_base		sensor;
        struct camera2_flash_dm_base		flash;
        struct camera2_hotpixel_dm		hotpixel;
        struct camera2_demosaic_dm		demosaic;
        struct camera2_noise_reduction_dm	noise;
        struct camera2_shading_dm		shading;
        struct camera2_geometric_dm		geometric;
        struct camera2_color_correction_dm_base	color;
        struct camera2_tonemap_dm		tonemap;
        struct camera2_edge_dm			edge;
        struct camera2_scaler_dm_base		scaler;
        struct camera2_jpeg_dm			jpeg;
        struct camera2_stats_dm_base		stats;
        struct camera2_aa_dm			aa;
};

struct camera2_dm {
	struct camera2_request_dm		request;
	struct camera2_lens_dm			lens;
	struct camera2_sensor_dm		sensor;
	struct camera2_flash_dm			flash;
	struct camera2_hotpixel_dm		hotpixel;
	struct camera2_demosaic_dm		demosaic;
	struct camera2_noise_reduction_dm	noise;
	struct camera2_shading_dm		shading;
	struct camera2_geometric_dm		geometric;
	struct camera2_color_correction_dm	color;
	struct camera2_tonemap_dm		tonemap;
	struct camera2_edge_dm			edge;
	struct camera2_scaler_dm		scaler;
	struct camera2_jpeg_dm			jpeg;
	struct camera2_stats_dm			stats;
	struct camera2_aa_dm			aa;
        struct camera2_entry_dm			entry; /* metadata v2 */
};

struct camera2_sm_base {
        struct camera2_lens_sm			lens;
        struct camera2_sensor_sm		sensor;
        struct camera2_flash_sm			flash;
        struct camera2_color_correction_sm_base	color;
        struct camera2_tonemap_sm		tonemap;
        struct camera2_scaler_sm		scaler;
        struct camera2_jpeg_sm			jpeg;
        struct camera2_stats_sm			stats;
        struct camera2_aa_sm			aa;

        /* User-defined(ispfw specific) static metadata. */
        struct camera2_lens_usm			lensud;
        struct camera2_sensor_usm		sensor_ud;
        struct camera2_flash_usm		flash_ud;
};

struct camera2_sm {
	struct camera2_lens_sm			lens;
	struct camera2_sensor_sm		sensor;
	struct camera2_flash_sm			flash;
	struct camera2_color_correction_sm	color;
	struct camera2_tonemap_sm		tonemap;
	struct camera2_scaler_sm		scaler;
	struct camera2_jpeg_sm			jpeg;
	struct camera2_stats_sm			stats;
	struct camera2_aa_sm			aa;

	/* User-defined(ispfw specific) static metadata. */
	struct camera2_lens_usm			lensud;
	struct camera2_sensor_usm		sensor_ud;
	struct camera2_flash_usm		flash_ud;
};

/*
 * User-defined control for lens.
 */
struct camera2_lens_uctl_base {

	struct camera2_lens_ctl ctl;
        /* It depends on the af algorithm(normally 255 or 1023) or normally 8, 9 or 10 */
        uint32_t		max_pos;
        /* It depends on the af algorithm.
           Some actuator support slew rate control. */
        uint32_t		slew_rate;
};

struct camera2_lens_uctl {
        struct camera2_lens_ctl ctl;
        /* It depends on max_pos */
        uint32_t pos; /* metadata v2 */
        /* It depends on the af algorithm(normally 255 or 1023) or normally 8, 9 or 10 */
        uint32_t		max_pos;
        /* It depends on the af algorithm */
        uint32_t direction; /* metadata v2 */
	/* Some actuators support slew rate control. */
        uint32_t		slew_rate;
};

/*
 * User-defined metadata for lens.
 */
struct camera2_lens_udm {
        /* It depends by posSize */
        uint32_t        pos;
        /* It depends by af algorithm(AF pos bit. normally 8 or 9 or 10) */
        uint32_t        posSize;
        /* It depends by af algorithm */
        uint32_t        direction;
        /* Some actuators support slew rate control. */
        uint32_t        slewRate;
};

/**
 * User-defined metadata for ae
 */
struct camera2_ae_udm {
        /* Vendor specific length */
        uint32_t	vs_length;
        /* vendor specific data array */
        uint32_t	vs_data[CAMERA2_MAX_VENDER_LENGTH];
};

/**
 * User-defined metadata for AWB
 */
struct camera2_awb_udm {
        uint32_t	vs_length;
        uint32_t	vd_data[CAMERA2_MAX_VENDER_LENGTH];
};

/**
 * User-defined metadata for AF
 */
struct camera2_af_udm {
        uint32_t	vs_length;
        uint32_t	vs_data[CAMERA2_MAX_VENDER_LENGTH];
        uint32_t	lens_pos_infinity;
        uint32_t	lens_pos_macro;
        uint32_t	lens_pos_current;
};

/**
 * User-defined metadata for AS (anti-shading)
 */
struct camera2_as_udm {
        uint32_t	vs_length;
        uint32_t 	vs_data[CAMERA2_MAX_VENDER_LENGTH];
};

/**
 * User-defined metadata for ipc
 */
struct camera2_ipc_udm {
        uint32_t	vs_length;
        uint32_t 	vs_data[CAMERA2_MAX_VENDER_LENGTH];
};

/**
 *
 */
struct camera2_internal_udm {
        uint32_t 	vs_data1[CAMERA2_MAX_VENDER_LENGTH];
        uint32_t 	vs_data2[CAMERA2_MAX_VENDER_LENGTH];
};

struct camera2_bayer_udm {
        uint32_t	width;
        uint32_t	height;
};


enum companion_drc_mode {
        COMPANION_DRC_OFF = 1,
        COMPANION_DRC_ON,
};

enum companion_wdr_mode {
        COMPANION_WDR_OFF = 1,
        COMPANION_WDR_ON,
};

enum companion_paf_mode {
        COMPANION_PAF_OFF = 1,
        COMPANION_PAF_ON,
};

struct camera2_pdaf_single_result {
        uint16_t	mode;
        uint16_t	goal_pos;
        uint16_t	reliability;
        uint16_t	current_pos;
};

struct camera2_pdaf_multi_result {
        uint16_t	mode;
        uint16_t	goal_pos;
        uint16_t	reliability;
};

struct camera2_pdaf_udm {
        uint16_t		num_column;
        uint16_t		num_row;
        struct camera2_pdaf_multi_result multi_result[CAMERA2_MAX_PDAF_MULTIROI_COLUMN][CAMERA2_MAX_PDAF_MULTIROI_ROW];
        struct camera2_pdaf_single_result	single_result;
        uint16_t		lens_pos_resolution;
};

struct camera2_companion_udm {
        enum companion_drc_mode		drc_mode;
        enum companion_wdr_mode		wdr_mode;
        enum companion_paf_mode		paf_mode;
        struct camera2_pdaf_udm		pdaf;
};

/*
 * User-defined control for sensor.
 */
struct camera2_sensor_uctl {
        struct camera2_sensor_uctl_base {
                struct camera2_sensor_ctl ctl;
                /* Dynamic frame duration.
                 * This feature is set to max. value between
                 * 'sensor.exposureTime'+ alpha and 'sensor.frameDuration'.
                 */
                uint64_t        dynamic_frame_duration;
        }base;
        uint32_t analogGain; /* metadata v2 */
        uint32_t digitalGain; /* metadata v2 */
        uint64_t longExposureTime; /* For supporting WDR */ /* metadata v2 */
        uint64_t shortExposureTime; /* metadata v2 */
        uint32_t longAnalogGain; /* metadata v2 */
        uint32_t shortAnalogGain; /* metadata v2 */
        uint32_t longDigitalGain; /* metadata v2 */
        uint32_t shortDigitalGain; /* metadata v2 */
};

struct camera2_scaler_uctl {
        struct camera2_scaler_uctl_base {
                /* Next frame target address, where '0' denotes invalid one. */
                uint32_t scc_target_address[4];
                uint32_t scp_target_address[4];
        }base;
        uint32_t dis_target_address[4]; /* metadata v2 */
        uint32_t taap_target_address[4]; /* 3AA preview DMA */ /* metadata v2 */
        uint32_t taac_target_address[4]; /* 3AA capture DMA */ /* metadata v2 */
        uint32_t orientation; /* metadata v2 */
};

struct camera2_flash_uctl {
	struct camera2_flash_ctl ctl;
};

struct camera2_bayer_uctl {
        struct camera2_scaler_ctl ctl;
};

struct camera2_companion_uctl {
        enum companion_drc_mode	drc_mode;
        enum companion_wdr_mode	wdr_mode;
        enum companion_paf_mode	paf_mode;
};

struct camera2_uctl_base {
        /* Set sensor, lens, flash control for next frame.
         * This flag can be combined.
         * [0 bit] lens
         * [1 bit] sensor
         * [2 bit] flash
         */
        uint32_t u_update_bitmap;

        /* For debugging */
        uint32_t u_frame_number;

        /* isp fw specific control (user-defined) of lens. */
        struct camera2_lens_uctl_base	lens_ud;
        /* isp fw specific control (user-defined) of sensor. */
        struct camera2_sensor_uctl_base	sensor_ud;
        /* isp fw specific control (user-defined) of flash. */
        struct camera2_flash_uctl	flash_ud;

        struct camera2_scaler_uctl_base	scaler_ud;

};

struct camera2_uctl {
	/* Set sensor, lens, flash control for next frame.
	 * This flag can be combined.
	 * [0 bit] lens
	 * [1 bit] sensor
	 * [2 bit] flash
	 */
	uint32_t u_update_bitmap;

	/* For debugging */
	uint32_t u_frame_number;

	/* isp fw specific control (user-defined) of lens. */
	struct camera2_lens_uctl	lens_ud;
	/* isp fw specific control (user-defined) of sensor. */
	struct camera2_sensor_uctl	sensor_ud;
	/* isp fw specific control (user-defined) of flash. */
	struct camera2_flash_uctl	flash_ud;

	struct camera2_scaler_uctl	scaler_ud;

        struct camera2_bayer_uctl	bayer_ud;

        struct camera2_companion_uctl	companion_ud;
};

struct camera2_udm {
	struct camera2_lens_udm		lens;
        struct camera2_ae_udm		ae;
        struct camera2_awb_udm		awb;
        struct camera2_af_udm           af;
        struct camera2_as_udm		as;
        struct camera2_ipc_udm		ipc;
        struct camera2_internal_udm	internal;
        struct camera2_bayer_udm	bayer;
        struct camera2_companion_udm	companion;
};

struct camera2_shot_base {
        /* standard area */
        struct camera2_ctl_base		ctl;
        struct camera2_dm_base		dm;
        /* user defined area */
        struct camera2_uctl_base	uctl;
        struct camera2_udm		udm;
        /* magic : 23456789 */
        uint32_t			magicnumber;
};

struct camera2_shot {
	/* standard area */
	struct camera2_ctl	ctl;
	struct camera2_dm	dm;
	/* user defined area */
	struct camera2_uctl	uctl;
	struct camera2_udm	udm;
	/* magic : 23456789 */
	uint32_t		magicnumber;
};


#endif

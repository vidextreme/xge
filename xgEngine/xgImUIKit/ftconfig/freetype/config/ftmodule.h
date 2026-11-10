/*
 * xgImUIKit FreeType module list. Overrides the default <freetype/config/ftmodule.h> because the ftconfig folder
 * comes first in the include path. Only what the editor fonts need: TrueType outlines, the smooth rasterizer and
 * the auto-hinter used by light hinting.
 */

FT_USE_MODULE( FT_Module_Class, autofit_module_class )
FT_USE_MODULE( FT_Driver_ClassRec, tt_driver_class )
FT_USE_MODULE( FT_Module_Class, psnames_module_class )
FT_USE_MODULE( FT_Module_Class, sfnt_module_class )
FT_USE_MODULE( FT_Renderer_Class, ft_smooth_renderer_class )

/* EOF */

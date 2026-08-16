#include <math.h>
#include <FViz/Core/FVizError.h>
#include <FViz/Rendering/FVizTextProperty.h>
#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Rendering/FVizTextPropertyPrivate.h>

static void fviz_text_property_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_text_property_class = {
    FVIZ_TYPE_TEXT_PROPERTY, "FVizTextProperty", &g_fviz_object_class,
    fviz_text_property_destroy, NULL
};
static float fviz_clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
static void fviz_text_property_destroy(FVizObject* object)
{
    FVizTextProperty* property = (FVizTextProperty*)object;
    fviz_release(property->font);
    property->font = NULL;
}
FVizResult fviz_text_property_create(FVizTextProperty** out_property)
{
    FVizTextProperty* property;
    if (out_property == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_property must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_property = NULL;
    property = (FVizTextProperty*)fviz_internal_object_allocate(sizeof(*property), &g_fviz_text_property_class, NULL);
    if (property == NULL) return fviz_last_error_code();
    if (fviz_font_create_builtin(&property->font) != FVIZ_OK)
    {
        fviz_release(property);
        return fviz_last_error_code();
    }
    property->font_size = 14.0f;
    property->color[0]=1.0f; property->color[1]=1.0f; property->color[2]=1.0f; property->color[3]=1.0f;
    property->background[0]=0.0f; property->background[1]=0.0f; property->background[2]=0.0f; property->background[3]=0.0f;
    property->horizontal_alignment = FVIZ_TEXT_ALIGN_LEFT;
    property->vertical_alignment = FVIZ_TEXT_ALIGN_BOTTOM;
    property->line_spacing = 1.0f;
    property->shadow = FVIZ_FALSE;
    property->shadow_offset[0]=1.0f; property->shadow_offset[1]=1.0f; property->shadow_opacity=0.5f;
    *out_property = property;
    return FVIZ_OK;
}
FVizResult fviz_text_property_set_font(FVizTextProperty* property, FVizFont* font)
{
    if (property == NULL || font == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (property->font == font) return FVIZ_OK;
    if (fviz_retain(font) == NULL) return fviz_last_error_code();
    fviz_release(property->font); property->font = font; fviz_object_modified((FVizObject*)property); return FVIZ_OK;
}
FVizFont* fviz_text_property_font(FVizTextProperty* property) { return property != NULL ? property->font : NULL; }
const FVizFont* fviz_text_property_const_font(const FVizTextProperty* property) { return property != NULL ? property->font : NULL; }
void fviz_text_property_set_font_size(FVizTextProperty* p,float s){ if(p==NULL)return; if(!isfinite(s)||s<1.0f)s=1.0f; if(s>512.0f)s=512.0f; if(p->font_size!=s){p->font_size=s;fviz_object_modified((FVizObject*)p);} }
float fviz_text_property_font_size(const FVizTextProperty* p){return p!=NULL?p->font_size:0.0f;}
void fviz_text_property_set_color(FVizTextProperty* p,float r,float g,float b,float a){if(p==NULL)return;r=fviz_clamp01(r);g=fviz_clamp01(g);b=fviz_clamp01(b);a=fviz_clamp01(a);if(p->color[0]!=r||p->color[1]!=g||p->color[2]!=b||p->color[3]!=a){p->color[0]=r;p->color[1]=g;p->color[2]=b;p->color[3]=a;fviz_object_modified((FVizObject*)p);}}
void fviz_text_property_get_color(const FVizTextProperty*p,float*r,float*g,float*b,float*a){if(p==NULL)return;if(r)*r=p->color[0];if(g)*g=p->color[1];if(b)*b=p->color[2];if(a)*a=p->color[3];}
void fviz_text_property_set_background(FVizTextProperty* p,float r,float g,float b,float a){if(p==NULL)return;r=fviz_clamp01(r);g=fviz_clamp01(g);b=fviz_clamp01(b);a=fviz_clamp01(a);if(p->background[0]!=r||p->background[1]!=g||p->background[2]!=b||p->background[3]!=a){p->background[0]=r;p->background[1]=g;p->background[2]=b;p->background[3]=a;fviz_object_modified((FVizObject*)p);}}
void fviz_text_property_get_background(const FVizTextProperty*p,float*r,float*g,float*b,float*a){if(p==NULL)return;if(r)*r=p->background[0];if(g)*g=p->background[1];if(b)*b=p->background[2];if(a)*a=p->background[3];}
void fviz_text_property_set_horizontal_alignment(FVizTextProperty*p,FVizTextHorizontalAlignment a){if(p==NULL||a<FVIZ_TEXT_ALIGN_LEFT||a>FVIZ_TEXT_ALIGN_RIGHT)return;if(p->horizontal_alignment!=a){p->horizontal_alignment=a;fviz_object_modified((FVizObject*)p);}}
FVizTextHorizontalAlignment fviz_text_property_horizontal_alignment(const FVizTextProperty*p){return p!=NULL?p->horizontal_alignment:FVIZ_TEXT_ALIGN_LEFT;}
void fviz_text_property_set_vertical_alignment(FVizTextProperty*p,FVizTextVerticalAlignment a){if(p==NULL||a<FVIZ_TEXT_ALIGN_BOTTOM||a>FVIZ_TEXT_ALIGN_TOP)return;if(p->vertical_alignment!=a){p->vertical_alignment=a;fviz_object_modified((FVizObject*)p);}}
FVizTextVerticalAlignment fviz_text_property_vertical_alignment(const FVizTextProperty*p){return p!=NULL?p->vertical_alignment:FVIZ_TEXT_ALIGN_BOTTOM;}
void fviz_text_property_set_line_spacing(FVizTextProperty*p,float f){if(p==NULL)return;if(!isfinite(f)||f<0.5f)f=0.5f;if(f>4.0f)f=4.0f;if(p->line_spacing!=f){p->line_spacing=f;fviz_object_modified((FVizObject*)p);}}
float fviz_text_property_line_spacing(const FVizTextProperty*p){return p!=NULL?p->line_spacing:1.0f;}
void fviz_text_property_set_shadow(FVizTextProperty*p,FVizBool e,float x,float y,float o){if(p==NULL)return;e=e!=FVIZ_FALSE?FVIZ_TRUE:FVIZ_FALSE;o=fviz_clamp01(o);if(!isfinite(x))x=0.0f;if(!isfinite(y))y=0.0f;if(p->shadow!=e||p->shadow_offset[0]!=x||p->shadow_offset[1]!=y||p->shadow_opacity!=o){p->shadow=e;p->shadow_offset[0]=x;p->shadow_offset[1]=y;p->shadow_opacity=o;fviz_object_modified((FVizObject*)p);}}
FVizBool fviz_text_property_shadow(const FVizTextProperty*p){return p!=NULL?p->shadow:FVIZ_FALSE;}
void fviz_text_property_get_shadow(const FVizTextProperty*p,float*x,float*y,float*o){if(p==NULL)return;if(x)*x=p->shadow_offset[0];if(y)*y=p->shadow_offset[1];if(o)*o=p->shadow_opacity;}

/*****************************************************************************
 * Sticker Datatypes
 * **************************************************************************/

#ifndef GENCODECS_JSON_ENCODER_fluxer_tags
#define GENCODECS_JSON_ENCODER_fluxer_tags(b, buf, size, _var, _type)         \
    {                                                                         \
        if (0 > (code = jsonb_array_auto(b, buf, size))) return code;         \
        if (_var != NULL && *_var != '\0') {                                  \
            const char *_p = _var;                                            \
            while (*_p != '\0') {                                             \
                while (*_p == ' ' || *_p == ',') ++_p;                        \
                if (*_p == '\0') break;                                       \
                const char *_end = strchr(_p, ',');                           \
                size_t _len = _end ? (size_t)(_end - _p) : strlen(_p);        \
                while (_len > 0 && _p[_len - 1] == ' ') --_len;               \
                if (_len > 0) {                                               \
                    if (0 > (code = jsonb_string_auto(b, buf, size, _p,       \
                                                      _len)))                 \
                        return code;                                          \
                }                                                             \
                if (!_end) break;                                             \
                _p = _end + 1;                                                \
            }                                                                 \
        }                                                                     \
        if (0 > (code = jsonb_array_pop_auto(b, buf, size))) return code;     \
    }
#endif

#ifndef GENCODECS_JSON_DECODER_fluxer_tags
#define GENCODECS_JSON_DECODER_fluxer_tags(_f, _js, _var, _type)              \
    if (_f && _f->v->type == JSMN_STRING) {                                   \
        GENCODECS_JSON_DECODER_PTR_char(_f, _js, _var, _type);                \
    }                                                                         \
    else if (_f && _f->v->type == JSMN_ARRAY) {                               \
        size_t _cap = 1;                                                      \
        int _k;                                                               \
        for (_k = 0; _k < _f->length; ++_k) {                                 \
            const jsmnf_pair *_item = _f->fields + _k;                        \
            if (_item->v->type == JSMN_STRING)                                \
                _cap += (size_t)(_item->v->end - _item->v->start) + 1;        \
        }                                                                     \
        if (!(_var = calloc(1, _cap))) return JSMN_ERROR_NOMEM;               \
        size_t _off = 0;                                                      \
        for (_k = 0; _k < _f->length; ++_k) {                                 \
            const jsmnf_pair *_item = _f->fields + _k;                        \
            if (_item->v->type != JSMN_STRING) continue;                      \
            if (_off > 0) _var[_off++] = ',';                                 \
            long _u = jsmnf_unescape(_var + _off,                             \
                                     (size_t)(_item->v->end                   \
                                              - _item->v->start),             \
                                     _js + _item->v->start,                   \
                                     (size_t)(_item->v->end                   \
                                              - _item->v->start));            \
            if (_u < 0) return free(_var), (_var = NULL), _u;                 \
            _off += (size_t)_u;                                               \
        }                                                                     \
        _var[_off] = '\0';                                                    \
        ret += (long)_off + 1;                                                \
    }
#endif

#if GENCODECS_RECIPE == DATA
ENUM(discord_sticker_types)
  /** an official sticker in a pack, part of Nitro or in a removed
       purchasable pack */
    ENUMERATOR(DISCORD_STICKER_STANDARD, = 1)
  /** a sticker uploaded to a Boosted guild for the guild's members */
    ENUMERATOR_LAST(DISCORD_STICKER_GUILD, = 2)
ENUM_END
#endif

#if GENCODECS_RECIPE == DATA
ENUM(discord_sticker_format_types)
    ENUMERATOR(DISCORD_STICKER_FORMAT_PNG, = 1)
    ENUMERATOR(DISCORD_STICKER_FORMAT_APNG, = 2)
    ENUMERATOR_LAST(DISCORD_STICKER_FORMAT_LOTTIE, = 3)
ENUM_END
#endif

/** @CCORD_pub_struct{discord_sticker} */
#if GENCODECS_RECIPE & (DATA | JSON)
PUB_STRUCT(discord_sticker)
  /** ID of the sticker */
    FIELD_SNOWFLAKE(id)
  /** for standard stickers, ID of the pack the sticker is from */
  COND_WRITE(self->pack_id != 0)
    FIELD_SNOWFLAKE(pack_id)
  COND_END
  /** name of the sticker */
    FIELD_PTR(name, char, *)
  /** description of the sticker */
    FIELD_PTR(description, char, *)
  /** autocomplete/suggestion tags for the sticker (max 200 characters) */
  COND_WRITE(self->tags != NULL)
    FIELD_CUSTOM(tags, "tags", char, *, INIT_BLANK, CLEANUP_PTR,
                 GENCODECS_JSON_ENCODER_fluxer_tags,
                 GENCODECS_JSON_DECODER_fluxer_tags, NULL)
  COND_END
  /** whether the sticker image is animated */
    FIELD(animated, bool, false)
  /** type of sticker */
  COND_WRITE(self->type != 0)
    FIELD_ENUM(type, discord_sticker_types)
  COND_END
  /** type of sticker format */
  COND_WRITE(self->format_type != 0)
    FIELD_ENUM(format_type, discord_sticker_format_types)
  COND_END
  /** whether this guild sticker can be used, may be false due to loss of
       Server Boosts */
    FIELD(available, bool, false)
  /** ID of the guild that owns this sticker */
  COND_WRITE(self->guild_id != 0)
    FIELD_SNOWFLAKE(guild_id)
  COND_END
  /** the user that uploaded the guild sticker */
  COND_WRITE(self->user != NULL)
    FIELD_STRUCT_PTR(user, discord_user, *)
  COND_END
  /** the standard sticker's sort order within its pack */
    FIELD(sort_value, int, 0)
STRUCT_END
#endif

/** @CCORD_pub_list{discord_stickers} */
#if GENCODECS_RECIPE & (DATA | JSON)
PUB_LIST(discord_stickers)
    LISTTYPE_STRUCT(discord_sticker)
LIST_END
#endif

#if GENCODECS_RECIPE & (DATA | JSON)
STRUCT(discord_sticker_item)
  /** ID of the sticker */
    FIELD_SNOWFLAKE(id)
  /** name of the sticker */
    FIELD_PTR(name, char, *)
  /** type of sticker format */
  COND_WRITE(self->format_type != 0)
    FIELD_ENUM(format_type, discord_sticker_format_types)
  COND_END
STRUCT_END
#endif

#if GENCODECS_RECIPE & (DATA | JSON)
LIST(discord_sticker_items)
    LISTTYPE_STRUCT(discord_sticker_item)
LIST_END
#endif

#if GENCODECS_RECIPE & (DATA | JSON)
STRUCT(discord_sticker_pack)
  /** ID of the sticker */
    FIELD_SNOWFLAKE(id)
  /** the stickers in the pack */
  COND_WRITE(self->stickers != NULL)
    FIELD_STRUCT_PTR(stickers, discord_stickers, *)
  COND_END
  /** name of the sticker pack */
    FIELD_PTR(name, char, *)
  /** ID of the pack's SKU */
    FIELD_SNOWFLAKE(sku_id)
  /** ID of a sticker in the pack which is shown as the pack's icon */
  COND_WRITE(self->cover_sticker_id != 0)
    FIELD_SNOWFLAKE(cover_sticker_id)
  COND_END
  /** description of the sticker pack */
    FIELD_PTR(description, char, *)
  /** ID of the sticker pack's banner image */
  COND_WRITE(self->banner_asset_id != 0)
    FIELD_SNOWFLAKE(banner_asset_id)
  COND_END
STRUCT_END
#endif

/** @CCORD_pub_struct{discord_list_nitro_sticker_packs} */
#if GENCODECS_RECIPE & (DATA | JSON)
PUB_LIST(discord_sticker_packs)
    LISTTYPE_STRUCT(discord_sticker_pack)
LIST_END
#endif

/*****************************************************************************
 * Sticker REST parameters
 * **************************************************************************/

/** @CCORD_pub_struct{discord_list_nitro_sticker_packs} */
#if GENCODECS_RECIPE & (DATA | JSON_DECODER)
PUB_STRUCT(discord_list_nitro_sticker_packs)
  /** array of sticker pack objects */
    FIELD_STRUCT_PTR(sticker_packs, discord_sticker_packs, *)
STRUCT_END
#endif

#if GENCODECS_RECIPE == DATA
STRUCT(discord_create_guild_sticker)
  /** @CCORD_reason{reason} */
    FIELD_PTR(reason, char, *)
  /** name of the sticker (2-30 characters) */
    FIELD_PTR(name, char, *)
  /** description of the sticker (empty or 2-100 characters) */
    FIELD_PTR(description, char, *)
  /** autocomplete/suggestion tags for the sticker (max 200 characters) */
    FIELD_PTR(tags, char, *)
  /** the sticker file to upload, must be a PNG, APNG, or Lottie JSON file
       max 500 KB */
    FIELD_STRUCT_PTR(file, discord_attachment, *)
STRUCT_END
#endif

/** @CCORD_pub_struct{discord_modify_guild_sticker} */
#if GENCODECS_RECIPE & (DATA | JSON)
PUB_STRUCT(discord_modify_guild_sticker)
  /** @CCORD_reason{reason} */
#if GENCODECS_RECIPE == DATA
    FIELD_PTR(reason, char, *)
#endif
  /** name of the sticker (2-30 characters) */
  COND_WRITE(self->name != NULL)
    FIELD_PTR(name, char, *)
  COND_END
  /** description of the sticker (empty or 2-100 characters) */
  COND_WRITE(self->description != NULL)
    FIELD_PTR(description, char, *)
  COND_END
  /** autocomplete/suggestion tags for the sticker (max 200 characters) */
  COND_WRITE(self->tags != NULL)
    FIELD_CUSTOM(tags, "tags", char, *, INIT_BLANK, CLEANUP_PTR,
                 GENCODECS_JSON_ENCODER_fluxer_tags,
                 GENCODECS_JSON_DECODER_fluxer_tags, NULL)
  COND_END
STRUCT_END
#endif

#if GENCODECS_RECIPE == DATA
STRUCT(discord_delete_guild_sticker)
  /** @CCORD_reason{reason} */
    FIELD_PTR(reason, char, *)
STRUCT_END
#endif

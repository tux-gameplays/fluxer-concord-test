/*****************************************************************************
 * Webhook Datatypes
 * **************************************************************************/

#if GENCODECS_RECIPE == DATA
ENUM(discord_webhook_types)
  /** Incoming Webhooks can post messages to channels with a generated token */
    ENUMERATOR(DISCORD_WEBHOOK_INCOMING, = 1)
  /** Channel Follower Webhooks are internal webhooks used with Channel
       Following to post new messages into channels */
    ENUMERATOR(DISCORD_WEBHOOK_CHANNEL_FOLLOWER, = 2)
  /** Application webhooks are webhooks used with Interactions */
    ENUMERATOR_LAST(DISCORD_WEBHOOK_APPLICATION, = 3)
ENUM_END
#endif

/** @CCORD_pub_struct{discord_webhook} */
#if GENCODECS_RECIPE & (DATA | JSON)
PUB_STRUCT(discord_webhook)
  /** the ID of the webhook */
    FIELD_SNOWFLAKE(id)
  /** the type of the webhook */
  COND_WRITE(self->type != 0)
    FIELD_ENUM(type, discord_webhook_types)
  COND_END
  /** the guild ID this webhook is for, if any */
  COND_WRITE(self->guild_id != 0)
    FIELD_SNOWFLAKE(guild_id)
  COND_END
  /** the channel ID this webhook is for, if any */
  COND_WRITE(self->channel_id != 0)
    FIELD_SNOWFLAKE(channel_id)
  COND_END
  /** the user this webhook was created by (not returned when getting a
       webhook with its token) */
  COND_WRITE(self->user != NULL)
    FIELD_STRUCT_PTR(user, discord_user, *)
  COND_END
  /** the default name of the webhook */
  COND_WRITE(self->name != NULL)
    FIELD_PTR(name, char, *)
  COND_END
  /** the default user avatar hash of the webhook */
  COND_WRITE(self->avatar != NULL)
    FIELD_PTR(avatar, char, *)
  COND_END
  /** the secure token of the webhook (returned for Incoming Webhooks) */
  COND_WRITE(self->token != NULL)
    FIELD_PTR(token, char, *)
  COND_END
  /** the bot/OAuth2 application that created this webhook */
  COND_WRITE(self->application_id != 0)
    FIELD_SNOWFLAKE(application_id)
  COND_END
  /** the guild of the channel that this webhook is following (returned for
       Channel Follower Webhooks) */
  COND_WRITE(self->source_guild != NULL)
    FIELD_STRUCT_PTR(source_guild, discord_guild, *)
  COND_END
  /** the channel that this webhook is following (returned for
       Channel Follower Webhooks) */
  COND_WRITE(self->source_channel != NULL)
    FIELD_STRUCT_PTR(source_channel, discord_channel, *)
  COND_END
  /** the url used for executing the webhook (returned by the webhooks
       OAuth2 flow */
  COND_WRITE(self->url != NULL)
    FIELD_PTR(url, char, *)
  COND_END
STRUCT_END
#endif

/** @CCORD_pub_list{discord_webhooks} */
#if GENCODECS_RECIPE & (DATA | JSON)
PUB_LIST(discord_webhooks)
    LISTTYPE_STRUCT(discord_webhook)
LIST_END
#endif

/*****************************************************************************
 * Webhook REST parameters
 * **************************************************************************/

/** @CCORD_pub_struct{discord_create_webhook} */
#if GENCODECS_RECIPE & (DATA | JSON)
PUB_STRUCT(discord_create_webhook)
  /** @CCORD_reason{reason} */
#if GENCODECS_RECIPE == DATA
    FIELD_PTR(reason, char, *)
#endif
  /** name of the webhook (1-80 characters) */
    FIELD_PTR(name, char, *)
  /** image for the default webhook avatar */
  COND_WRITE(self->avatar != NULL)
    FIELD_PTR(avatar, char, *)
  COND_END
STRUCT_END
#endif

/** @CCORD_pub_struct{discord_modify_webhook} */
#if GENCODECS_RECIPE & (DATA | JSON)
PUB_STRUCT(discord_modify_webhook)
  /** @CCORD_reason{reason} */
#if GENCODECS_RECIPE == DATA
    FIELD_PTR(reason, char, *)
#endif
  /** the default name of the webhook */
  COND_WRITE(self->name != NULL)
    FIELD_PTR(name, char, *)
  COND_END
  /** image for the default webhook avatar */
  COND_WRITE(self->avatar != NULL)
    FIELD_PTR(avatar, char, *)
  COND_END
  /** the new channel ID for this webhook should be moved to */
  COND_WRITE(self->channel_id != 0)
    FIELD_SNOWFLAKE(channel_id)
  COND_END
STRUCT_END
#endif

#if GENCODECS_RECIPE == DATA
STRUCT(discord_delete_webhook)
  /** @CCORD_reason{reason} */
    FIELD_PTR(reason, char, *)
STRUCT_END
#endif

/** @CCORD_pub_struct{discord_modify_webhook_with_token} */
#if GENCODECS_RECIPE & (DATA | JSON)
PUB_STRUCT(discord_modify_webhook_with_token)
  /** the default name of the webhook */
  COND_WRITE(self->name != NULL)
    FIELD_PTR(name, char, *)
  COND_END
  /** image for the default webhook avatar */
  COND_WRITE(self->avatar != NULL)
    FIELD_PTR(avatar, char, *)
  COND_END
STRUCT_END
#endif

/** @CCORD_pub_struct{discord_execute_webhook} */
#if GENCODECS_RECIPE & (DATA | JSON)
PUB_STRUCT(discord_execute_webhook)
  /* QUERY FIELDS */
#if !(GENCODECS_RECIPE & JSON)
  /** waits for server confirmation of message send before response, and
       returns the created message body (defaults to `false`; when `false` a
       message that is not saved does not return an error) */
    FIELD(wait, bool, false)
  /** send a message to the specified thread within a webhook's channel; the
       thread will automatically be unarchived */
    FIELD_SNOWFLAKE(thread_id)
#endif
  /* JSON FIELDS */
  /** the message contents (up to 2000 characters) */
  COND_WRITE(self->content != NULL)
    FIELD_PTR(content, char, *)
  COND_END
  /** override the default username of the webhook */
  COND_WRITE(self->username != NULL)
    FIELD_PTR(username, char, *)
  COND_END
  /** override the default avatar of the webhook */
  COND_WRITE(self->avatar_url != NULL)
    FIELD_PTR(avatar_url, char, *)
  COND_END
  /** true if this is a TTS message */
  COND_WRITE(self->tts != false)
    FIELD(tts, bool, false)
  COND_END
  /** embedded `rich` content */
  COND_WRITE(self->embeds != NULL)
    FIELD_STRUCT_PTR(embeds, discord_embeds, *)
  COND_END
  /** allowed mentions for the message */
  COND_WRITE(self->allowed_mentions != NULL)
    FIELD_STRUCT_PTR(allowed_mentions, discord_allowed_mention, *)
  COND_END
  /** the components to include with the message */
  COND_WRITE(self->components != NULL)
    FIELD_STRUCT_PTR(components, discord_components, *)
  COND_END
  /** attachment objects with filename and description */
  COND_WRITE(self->attachments != NULL)
    FIELD_STRUCT_PTR(attachments, discord_attachments, *)
  COND_END
  /** @ref DiscordAPIChannelMessageFlags combined as a bitfield (only
       `SUPPRESS_EMBEDS` can be set) */
  COND_WRITE(self->flags != 0)
    FIELD_BITMASK(flags)
  COND_END
STRUCT_END
#endif

#if GENCODECS_RECIPE == DATA
STRUCT(discord_get_webhook_message)
  /** ID of the thread the message is in */
  COND_WRITE(self->thread_id != 0)
    FIELD_SNOWFLAKE(thread_id)
  COND_END
STRUCT_END
#endif

/** @CCORD_pub_struct{discord_edit_webhook_message} */
#if GENCODECS_RECIPE & (DATA | JSON)
PUB_STRUCT(discord_edit_webhook_message)
  /* QUERY FIELDS */
#if !(GENCODECS_RECIPE & JSON)
  /** id of the thread the message is in */
    FIELD_SNOWFLAKE(thread_id)
#endif
  /* JSON FIELDS */
  /** the message contents (up to 2000 characters) */
  COND_WRITE(self->content != NULL)
    FIELD_PTR(content, char, *)
  COND_END
  /** embedded `rich` content */
  COND_WRITE(self->embeds != NULL)
    FIELD_STRUCT_PTR(embeds, discord_embeds, *)
  COND_END
  /** allowed mentions for the message */
  COND_WRITE(self->allowed_mentions != NULL)
    FIELD_STRUCT_PTR(allowed_mentions, discord_allowed_mention, *)
  COND_END
  /** the components to include with the message */
  COND_WRITE(self->components != NULL)
    FIELD_STRUCT_PTR(components, discord_components, *)
  COND_END
  /** attached files to keep and possible descriptions for new files */
  COND_WRITE(self->attachments != NULL)
    FIELD_STRUCT_PTR(attachments, discord_attachments, *)
  COND_END
  /** @ref DiscordAPIChannelMessageFlags combined as a bitfield */
  COND_WRITE(self->flags != 0)
    FIELD_BITMASK(flags)
  COND_END
STRUCT_END
#endif

#if GENCODECS_RECIPE == DATA
STRUCT(discord_delete_webhook_message)
  /** ID of the thread the message is in */
  COND_WRITE(self->thread_id != 0)
    FIELD_SNOWFLAKE(thread_id)
  COND_END
STRUCT_END
#endif

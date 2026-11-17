# Concord–Fluxer compatibility inventory

This is a per-public-function inventory of Concord's public C headers. `pending` means the operation has not yet been checked against the Fluxer API; the final compatibility check must reject any remaining pending entries. Slash/application-command and interaction APIs are intentionally marked unsupported unless Fluxer documents an equivalent.

| Concord function | Header | Status | Fluxer mapping / notes |
| --- | --- | --- | --- |
| `discord_add_guild_member` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_add_guild_member_role` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_add_intents` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_add_thread_member` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_begin_guild_prune` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_bulk_delete_messages` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_bulk_overwrite_global_application_commands` | `include/application_command.h` | unsupported | Fluxer currently has no compatible slash-command/interaction protocol. |
| `discord_bulk_overwrite_guild_application_commands` | `include/application_command.h` | unsupported | Fluxer currently has no compatible slash-command/interaction protocol. |
| `discord_cache_enable` | `include/discord-cache.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_cache_get_channel_message` | `include/discord-cache.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_cache_get_guild` | `include/discord-cache.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_claim` | `include/discord.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_cleanup` | `include/discord.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_clone` | `include/discord.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_code_as_string` | `include/discord.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_config_get_field` | `include/discord.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_create_auto_moderation_rule` | `include/auto_moderation.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_create_channel_invite` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_create_dm` | `include/user.h` | supported | `POST /v1/users/@me/channels` with `recipient_id`. |
| `discord_create_followup_message` | `include/interaction.h` | unsupported | Fluxer currently has no compatible slash-command/interaction protocol. |
| `discord_create_global_application_command` | `include/application_command.h` | unsupported | Fluxer currently has no compatible slash-command/interaction protocol. |
| `discord_create_group_dm` | `include/user.h` | unsupported | Concord's access-token/nickname payload has no equivalent to Fluxer's recipients-only bot request. |
| `discord_create_guild` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_create_guild_application_command` | `include/application_command.h` | unsupported | Fluxer currently has no compatible slash-command/interaction protocol. |
| `discord_create_guild_ban` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_create_guild_channel` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_create_guild_emoji` | `include/emoji.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_create_guild_from_guild_template` | `include/guild_template.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_create_guild_role` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_create_guild_scheduled_event` | `include/guild_scheduled_event.h` | unsupported | Fluxer has no guild scheduled-events (`/v1/guilds/{guild_id}/scheduled-events`) API. |
| `discord_create_guild_template` | `include/guild_template.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_create_interaction_response` | `include/interaction.h` | unsupported | Fluxer currently has no compatible slash-command/interaction protocol. |
| `discord_create_message` | `include/channel.h` | supported | `POST /v1/channels/{channel_id}/messages`; verified request/response and large snowflake decoding. |
| `discord_create_reaction` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_create_stage_instance` | `include/stage_instance.h` | unsupported | Fluxer has no stage-channel or `/v1/stage-instances` API. |
| `discord_create_webhook` | `include/webhook.h` | supported | `POST /v1/channels/{channel_id}/webhooks` with `name` and optional `avatar`. |
| `discord_crosspost_message` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_delete_all_reactions` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_delete_all_reactions_for_emoji` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_delete_auto_moderation_rule` | `include/auto_moderation.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_delete_channel` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_delete_channel_permission` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_delete_followup_message` | `include/interaction.h` | unsupported | Fluxer currently has no compatible slash-command/interaction protocol. |
| `discord_delete_global_application_command` | `include/application_command.h` | unsupported | Fluxer currently has no compatible slash-command/interaction protocol. |
| `discord_delete_guild` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_delete_guild_application_command` | `include/application_command.h` | unsupported | Fluxer currently has no compatible slash-command/interaction protocol. |
| `discord_delete_guild_emoji` | `include/emoji.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_delete_guild_integrations` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_delete_guild_role` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_delete_guild_scheduled_event` | `include/guild_scheduled_event.h` | unsupported | Fluxer has no guild scheduled-events (`/v1/guilds/{guild_id}/scheduled-events`) API. |
| `discord_delete_guild_sticker` | `include/sticker.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_delete_guild_template` | `include/guild_template.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_delete_invite` | `include/invite.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_delete_message` | `include/channel.h` | supported | `DELETE /v1/channels/{channel_id}/messages/{message_id}`. |
| `discord_delete_original_interaction_response` | `include/interaction.h` | unsupported | Fluxer currently has no compatible slash-command/interaction protocol. |
| `discord_delete_own_reaction` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_delete_stage_instance` | `include/stage_instance.h` | unsupported | Fluxer has no stage-channel or `/v1/stage-instances` API. |
| `discord_delete_user_reaction` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_delete_webhook` | `include/webhook.h` | supported | `DELETE /v1/webhooks/{webhook_id}`. |
| `discord_delete_webhook_message` | `include/webhook.h` | supported | `DELETE /v1/webhooks/{webhook_id}/{token}/messages/{message_id}` without `Authorization` header. |
| `discord_delete_webhook_with_token` | `include/webhook.h` | supported | `DELETE /v1/webhooks/{webhook_id}/{token}` without `Authorization` header. |
| `discord_disconnect_guild_member` | `include/gateway.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_edit_channel_permissions` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_edit_followup_message` | `include/interaction.h` | unsupported | Fluxer currently has no compatible slash-command/interaction protocol. |
| `discord_edit_global_application_command` | `include/application_command.h` | unsupported | Fluxer currently has no compatible slash-command/interaction protocol. |
| `discord_edit_guild_application_command` | `include/application_command.h` | unsupported | Fluxer currently has no compatible slash-command/interaction protocol. |
| `discord_edit_message` | `include/channel.h` | supported | `PATCH /v1/channels/{channel_id}/messages/{message_id}`. |
| `discord_edit_original_interaction_response` | `include/interaction.h` | unsupported | Fluxer currently has no compatible slash-command/interaction protocol. |
| `discord_edit_webhook_message` | `include/webhook.h` | supported | `PATCH /v1/webhooks/{webhook_id}/{token}/messages/{message_id}` without `Authorization` header; supports optional `?thread_id=`. |
| `discord_execute_webhook` | `include/webhook.h` | supported | `POST /v1/webhooks/{webhook_id}/{token}` without `Authorization` header; supports `?wait=true`, `?thread_id=`, JSON, and multipart uploads. |
| `discord_follow_news_channel` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_from_config` | `include/discord.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_from_json` | `include/discord.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_from_token` | `include/discord.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_application_command_permissions` | `include/application_command.h` | unsupported | Fluxer currently has no compatible slash-command/interaction protocol. |
| `discord_get_auto_moderation_rule` | `include/auto_moderation.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_channel` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_channel_at_pos` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_channel_invites` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_channel_message` | `include/channel.h` | supported | `GET /v1/channels/{channel_id}/messages/{message_id}`. |
| `discord_get_channel_messages` | `include/channel.h` | supported | `GET /v1/channels/{channel_id}/messages`. |
| `discord_get_channel_webhooks` | `include/webhook.h` | supported | `GET /v1/channels/{channel_id}/webhooks`. |
| `discord_get_current_authorization_information` | `include/oauth2.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_current_bot_application_information` | `include/oauth2.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_current_user` | `include/user.h` | supported | `GET /v1/users/@me`; verified during client initialization. |
| `discord_get_current_user_guilds` | `include/user.h` | supported | `GET /v1/users/@me/guilds`. |
| `discord_get_data` | `include/discord.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_followup_message` | `include/interaction.h` | unsupported | Fluxer currently has no compatible slash-command/interaction protocol. |
| `discord_get_gateway` | `include/gateway.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_gateway_bot` | `include/gateway.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_global_application_command` | `include/application_command.h` | unsupported | Fluxer currently has no compatible slash-command/interaction protocol. |
| `discord_get_global_application_commands` | `include/application_command.h` | unsupported | Fluxer currently has no compatible slash-command/interaction protocol. |
| `discord_get_guild` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_guild_application_command` | `include/application_command.h` | unsupported | Fluxer currently has no compatible slash-command/interaction protocol. |
| `discord_get_guild_application_command_permissions` | `include/application_command.h` | unsupported | Fluxer currently has no compatible slash-command/interaction protocol. |
| `discord_get_guild_application_commands` | `include/application_command.h` | unsupported | Fluxer currently has no compatible slash-command/interaction protocol. |
| `discord_get_guild_audit_log` | `include/audit_log.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_guild_ban` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_guild_bans` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_guild_channels` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_guild_emoji` | `include/emoji.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_guild_integrations` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_guild_invites` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_guild_member` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_guild_preview` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_guild_prune_count` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_guild_roles` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_guild_scheduled_event` | `include/guild_scheduled_event.h` | unsupported | Fluxer has no guild scheduled-events (`/v1/guilds/{guild_id}/scheduled-events`) API. |
| `discord_get_guild_scheduled_event_users` | `include/guild_scheduled_event.h` | unsupported | Fluxer has no guild scheduled-events (`/v1/guilds/{guild_id}/scheduled-events`) API. |
| `discord_get_guild_sticker` | `include/sticker.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_guild_template` | `include/guild_template.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_guild_templates` | `include/guild_template.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_guild_vanity_url` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_guild_voice_regions` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_guild_webhooks` | `include/webhook.h` | supported | `GET /v1/guilds/{guild_id}/webhooks`. |
| `discord_get_guild_welcome_screen` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_guild_widget` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_guild_widget_image` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_guild_widget_settings` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_invite` | `include/invite.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_io_poller` | `include/discord.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_logmod` | `include/discord.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_original_interaction_response` | `include/interaction.h` | unsupported | Fluxer currently has no compatible slash-command/interaction protocol. |
| `discord_get_ping` | `include/discord.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_pinned_messages` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_reactions` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_self` | `include/discord.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_stage_instance` | `include/stage_instance.h` | unsupported | Fluxer has no stage-channel or `/v1/stage-instances` API. |
| `discord_get_sticker` | `include/sticker.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_user` | `include/user.h` | supported | `GET /v1/users/{user_id}`; verified including Fluxer 429 retry. |
| `discord_get_user_connections` | `include/user.h` | unsupported | Fluxer's route requires OAuth2 `connections` scope, not a bot token. |
| `discord_get_webhook` | `include/webhook.h` | supported | `GET /v1/webhooks/{webhook_id}`. |
| `discord_get_webhook_message` | `include/webhook.h` | supported | `GET /v1/webhooks/{webhook_id}/{token}/messages/{message_id}` without `Authorization` header. |
| `discord_get_webhook_with_token` | `include/webhook.h` | supported | `GET /v1/webhooks/{webhook_id}/{token}` without `Authorization` header. |
| `discord_group_dm_add_recipient` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_group_dm_remove_recipient` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_join_thread` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_leave_guild` | `include/user.h` | supported | `DELETE /v1/users/@me/guilds/{guild_id}`. |
| `discord_leave_thread` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_list_auto_moderation_rules_for_guild` | `include/auto_moderation.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_list_guild_emojis` | `include/emoji.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_list_guild_members` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_list_guild_scheduled_events` | `include/guild_scheduled_event.h` | unsupported | Fluxer has no guild scheduled-events (`/v1/guilds/{guild_id}/scheduled-events`) API. |
| `discord_list_guild_stickers` | `include/sticker.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_list_joined_private_archived_threads` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_list_nitro_sticker_packs` | `include/sticker.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_list_private_archived_threads` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_list_public_archived_threads` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_list_thread_members` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_list_voice_regions` | `include/voice.h` | unsupported | Fluxer has no global `/v1/voice/regions` route; `GET /v1/channels/{channel_id}/rtc-regions` is channel-scoped and disallows bots. |
| `discord_modify_auto_moderation_rule` | `include/auto_moderation.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_modify_channel` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_modify_current_member` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_modify_current_user` | `include/user.h` | unsupported | Fluxer bot profile updates require an application-scoped route/credentials absent from this Concord API. |
| `discord_modify_current_user_voice_state` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_modify_guild` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_modify_guild_channel_positions` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_modify_guild_emoji` | `include/emoji.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_modify_guild_member` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_modify_guild_role` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_modify_guild_role_positions` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_modify_guild_scheduled_event` | `include/guild_scheduled_event.h` | unsupported | Fluxer has no guild scheduled-events (`/v1/guilds/{guild_id}/scheduled-events`) API. |
| `discord_modify_guild_sticker` | `include/sticker.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_modify_guild_template` | `include/guild_template.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_modify_guild_welcome_screen` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_modify_guild_widget` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_modify_stage_instance` | `include/stage_instance.h` | unsupported | Fluxer has no stage-channel or `/v1/stage-instances` API. |
| `discord_modify_user_voice_state` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_modify_webhook` | `include/webhook.h` | supported | `PATCH /v1/webhooks/{webhook_id}` with optional `name`, `avatar`, and `channel_id`. |
| `discord_modify_webhook_with_token` | `include/webhook.h` | supported | `PATCH /v1/webhooks/{webhook_id}/{token}` without `Authorization` header. |
| `discord_overwrite_append` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_pin_message` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_presence_add_activity` | `include/gateway.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_reconnect` | `include/discord.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_remove_guild_ban` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_remove_guild_member` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_remove_guild_member_role` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_remove_intents` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_remove_thread_member` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_request_guild_members` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_run` | `include/discord.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_search_guild_members` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_data` | `include/discord.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_event_scheduler` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_application_command_permissions_update` | `include/discord-events.h` | unsupported | Fluxer currently has no compatible application-command permission event. |
| `discord_set_on_auto_moderation_action_execution` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_auto_moderation_rule_create` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_auto_moderation_rule_delete` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_auto_moderation_rule_update` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_channel_create` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_channel_delete` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_channel_pins_update` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_channel_update` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_command` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_commands` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_cycle` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_guild_ban_add` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_guild_ban_remove` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_guild_create` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_guild_delete` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_guild_emojis_update` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_guild_integrations_update` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_guild_member_add` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_guild_member_remove` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_guild_members_chunk` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_guild_member_update` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_guild_role_create` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_guild_role_delete` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_guild_role_update` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_guild_scheduled_event_create` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_guild_scheduled_event_delete` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_guild_scheduled_event_update` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_guild_scheduled_event_user_add` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_guild_scheduled_event_user_remove` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_guild_stickers_update` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_guild_update` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_idle` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_integration_create` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_integration_delete` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_integration_update` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_interaction_create` | `include/discord-events.h` | unsupported | Fluxer currently has no compatible slash-command interaction event. |
| `discord_set_on_invite_create` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_invite_delete` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_message_create` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_message_delete` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_message_delete_bulk` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_message_reaction_add` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_message_reaction_remove` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_message_reaction_remove_all` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_message_reaction_remove_emoji` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_message_update` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_presence_update` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_ready` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_stage_instance_create` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_stage_instance_delete` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_stage_instance_update` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_thread_create` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_thread_delete` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_thread_list_sync` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_thread_members_update` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_thread_member_update` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_thread_update` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_typing_start` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_user_update` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_voice_server_update` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_voice_state_update` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_on_webhooks_update` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_prefix` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_shutdown` | `include/discord.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_shutdown_all` | `include/discord.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_shutdown_all_ongoing` | `include/discord.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_start_thread_with_message` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_start_thread_without_message` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_strerror` | `include/discord.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_sync_guild_template` | `include/guild_template.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_timer` | `include/discord.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_timer_cancel` | `include/discord.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_timer_cancel_and_delete` | `include/discord.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_timer_ctl` | `include/discord.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_timer_delete` | `include/discord.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_timer_get` | `include/discord.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_timer_interval` | `include/discord.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_timer_start` | `include/discord.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_timer_stop` | `include/discord.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_timestamp` | `include/discord.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_timestamp_us` | `include/discord.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_trigger_typing_indicator` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_unclaim` | `include/discord.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_unpin_message` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_update_presence` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_update_voice_state` | `include/discord-events.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |

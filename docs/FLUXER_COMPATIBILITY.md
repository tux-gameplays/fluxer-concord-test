# Concord–Fluxer compatibility inventory

This is a per-public-function inventory of Concord's public C headers. `pending` means the operation has not yet been checked against the Fluxer API; the final compatibility check must reject any remaining pending entries. Slash/application-command and interaction APIs are intentionally marked unsupported unless Fluxer documents an equivalent.

| Concord function | Header | Status | Fluxer mapping / notes |
| --- | --- | --- | --- |
| `discord_add_guild_member` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_add_guild_member_role` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_add_intents` | `include/discord-events.h` | adapted | Source-compatible local bitmask setter; Fluxer Gateway v1 does not use or accept Discord intent bitmasks in `IDENTIFY`. |
| `discord_add_thread_member` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_begin_guild_prune` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_bulk_delete_messages` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_bulk_overwrite_global_application_commands` | `include/application_command.h` | unsupported | Fluxer currently has no compatible slash-command/interaction protocol. |
| `discord_bulk_overwrite_guild_application_commands` | `include/application_command.h` | unsupported | Fluxer currently has no compatible slash-command/interaction protocol. |
| `discord_cache_enable` | `include/discord-cache.h` | supported | Enables in-memory guild and message caching over Fluxer `READY`, `GUILD_*`, and `MESSAGE_*` Gateway events. |
| `discord_cache_get_channel_message` | `include/discord-cache.h` | supported | Returns a locally cached Fluxer `struct discord_message` matching `channel_id` and `message_id`. |
| `discord_cache_get_guild` | `include/discord-cache.h` | supported | Returns a locally cached Fluxer `struct discord_guild` matching `guild_id`. |
| `discord_claim` | `include/discord.h` | supported | Local reference-counting helper claiming ownership of Concord-managed event/cache objects. |
| `discord_cleanup` | `include/discord.h` | supported | Local client destructor releasing REST, Gateway, cache, timer, and Fluxer discovery resources. |
| `discord_clone` | `include/discord.h` | supported | Local client handle cloner preserving Fluxer `instance_origin` and discovered service URLs for worker threads. |
| `discord_code_as_string` | `include/discord.h` | supported | Local error-code name formatter (including `CCORD_UNSUPPORTED`). |
| `discord_config_get_field` | `include/discord.h` | supported | Local JSON configuration field reader for `discord_from_json()` clients. |
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
| `discord_create_guild_scheduled_event` | `include/guild_scheduled_event.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_create_guild_template` | `include/guild_template.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_create_interaction_response` | `include/interaction.h` | unsupported | Fluxer currently has no compatible slash-command/interaction protocol. |
| `discord_create_message` | `include/channel.h` | supported | `POST /v1/channels/{channel_id}/messages`; verified request/response and large snowflake decoding. |
| `discord_create_reaction` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_create_stage_instance` | `include/stage_instance.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_create_webhook` | `include/webhook.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
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
| `discord_delete_guild_scheduled_event` | `include/guild_scheduled_event.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_delete_guild_sticker` | `include/sticker.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_delete_guild_template` | `include/guild_template.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_delete_invite` | `include/invite.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_delete_message` | `include/channel.h` | supported | `DELETE /v1/channels/{channel_id}/messages/{message_id}`. |
| `discord_delete_original_interaction_response` | `include/interaction.h` | unsupported | Fluxer currently has no compatible slash-command/interaction protocol. |
| `discord_delete_own_reaction` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_delete_stage_instance` | `include/stage_instance.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_delete_user_reaction` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_delete_webhook` | `include/webhook.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_delete_webhook_message` | `include/webhook.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_delete_webhook_with_token` | `include/webhook.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_disconnect_guild_member` | `include/gateway.h` | supported | `PATCH /v1/guilds/{guild_id}/members/{user_id}` with `{"channel_id":null}`. |
| `discord_edit_channel_permissions` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_edit_followup_message` | `include/interaction.h` | unsupported | Fluxer currently has no compatible slash-command/interaction protocol. |
| `discord_edit_global_application_command` | `include/application_command.h` | unsupported | Fluxer currently has no compatible slash-command/interaction protocol. |
| `discord_edit_guild_application_command` | `include/application_command.h` | unsupported | Fluxer currently has no compatible slash-command/interaction protocol. |
| `discord_edit_message` | `include/channel.h` | supported | `PATCH /v1/channels/{channel_id}/messages/{message_id}`. |
| `discord_edit_original_interaction_response` | `include/interaction.h` | unsupported | Fluxer currently has no compatible slash-command/interaction protocol. |
| `discord_edit_webhook_message` | `include/webhook.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_execute_webhook` | `include/webhook.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_follow_news_channel` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_from_config` | `include/discord.h` | adapted | Initializes a Fluxer client from `struct discord_config` (`token`, optional `instance_origin`, logging) via `GET /.well-known/fluxer` and `GET /v1/users/@me`. |
| `discord_from_json` | `include/discord.h` | adapted | Initializes a Fluxer client from `config.json` (`token`, optional `instance_origin`, logging) via `GET /.well-known/fluxer` and `GET /v1/users/@me`. |
| `discord_from_token` | `include/discord.h` | adapted | Initializes a Fluxer client against the default official origin (`https://api.fluxer.app`) via `GET /.well-known/fluxer` and `GET /v1/users/@me`. |
| `discord_get_application_command_permissions` | `include/application_command.h` | unsupported | Fluxer currently has no compatible slash-command/interaction protocol. |
| `discord_get_auto_moderation_rule` | `include/auto_moderation.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_channel` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_channel_at_pos` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_channel_invites` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_channel_message` | `include/channel.h` | supported | `GET /v1/channels/{channel_id}/messages/{message_id}`. |
| `discord_get_channel_messages` | `include/channel.h` | supported | `GET /v1/channels/{channel_id}/messages`. |
| `discord_get_channel_webhooks` | `include/webhook.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_current_authorization_information` | `include/oauth2.h` | unsupported | Fluxer's `GET /v1/oauth2/@me` requires an OAuth2 Bearer access token rather than a bot token. |
| `discord_get_current_bot_application_information` | `include/oauth2.h` | adapted | Adapted from `/oauth2/applications/@me` to Fluxer `GET /v1/applications/@me`. |
| `discord_get_current_user` | `include/user.h` | supported | `GET /v1/users/@me`; verified during client initialization. |
| `discord_get_current_user_guilds` | `include/user.h` | supported | `GET /v1/users/@me/guilds`. |
| `discord_get_data` | `include/discord.h` | supported | Local getter for caller-supplied user data pointer. |
| `discord_get_followup_message` | `include/interaction.h` | unsupported | Fluxer currently has no compatible slash-command/interaction protocol. |
| `discord_get_gateway` | `include/gateway.h` | unsupported | Fluxer HTTP v1 has no `GET /v1/gateway` route; gateway URLs come from `GET /.well-known/fluxer` (`endpoints.gateway`) or `GET /v1/gateway/bot`. |
| `discord_get_gateway_bot` | `include/gateway.h` | supported | `GET /v1/gateway/bot`. |
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
| `discord_get_guild_scheduled_event` | `include/guild_scheduled_event.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_guild_scheduled_event_users` | `include/guild_scheduled_event.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_guild_sticker` | `include/sticker.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_guild_template` | `include/guild_template.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_guild_templates` | `include/guild_template.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_guild_vanity_url` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_guild_voice_regions` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_guild_webhooks` | `include/webhook.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_guild_welcome_screen` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_guild_widget` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_guild_widget_image` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_guild_widget_settings` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_invite` | `include/invite.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_io_poller` | `include/discord.h` | supported | Local getter for the client's `struct io_poller`. |
| `discord_get_logmod` | `include/discord.h` | supported | Local getter for the client's `logmod` logging context. |
| `discord_get_original_interaction_response` | `include/interaction.h` | unsupported | Fluxer currently has no compatible slash-command/interaction protocol. |
| `discord_get_ping` | `include/discord.h` | supported | Returns the latest Fluxer Gateway heartbeat round-trip latency in milliseconds. |
| `discord_get_pinned_messages` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_reactions` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_self` | `include/discord.h` | supported | Returns the cached `struct discord_user` fetched from Fluxer `GET /v1/users/@me`. |
| `discord_get_stage_instance` | `include/stage_instance.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_sticker` | `include/sticker.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_user` | `include/user.h` | supported | `GET /v1/users/{user_id}`; verified including Fluxer 429 retry. |
| `discord_get_user_connections` | `include/user.h` | unsupported | Fluxer's route requires OAuth2 `connections` scope, not a bot token. |
| `discord_get_webhook` | `include/webhook.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_webhook_message` | `include/webhook.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_get_webhook_with_token` | `include/webhook.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_group_dm_add_recipient` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_group_dm_remove_recipient` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_join_thread` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_leave_guild` | `include/user.h` | supported | `DELETE /v1/users/@me/guilds/{guild_id}`. |
| `discord_leave_thread` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_list_auto_moderation_rules_for_guild` | `include/auto_moderation.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_list_guild_emojis` | `include/emoji.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_list_guild_members` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_list_guild_scheduled_events` | `include/guild_scheduled_event.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_list_guild_stickers` | `include/sticker.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_list_joined_private_archived_threads` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_list_nitro_sticker_packs` | `include/sticker.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_list_private_archived_threads` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_list_public_archived_threads` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_list_thread_members` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_list_voice_regions` | `include/voice.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
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
| `discord_modify_guild_scheduled_event` | `include/guild_scheduled_event.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_modify_guild_sticker` | `include/sticker.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_modify_guild_template` | `include/guild_template.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_modify_guild_welcome_screen` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_modify_guild_widget` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_modify_stage_instance` | `include/stage_instance.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_modify_user_voice_state` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_modify_webhook` | `include/webhook.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_modify_webhook_with_token` | `include/webhook.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_overwrite_append` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_pin_message` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_presence_add_activity` | `include/gateway.h` | supported | Local helper appending an activity to a Gateway `PresenceUpdate` (opcode `3`) payload. |
| `discord_reconnect` | `include/discord.h` | supported | Triggers a Fluxer Gateway WebSocket close and reconnect (resuming via opcode `6` or re-identifying via opcode `2`). |
| `discord_remove_guild_ban` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_remove_guild_member` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_remove_guild_member_role` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_remove_intents` | `include/discord-events.h` | adapted | Source-compatible local bitmask modifier; Fluxer Gateway v1 does not use or accept Discord intent bitmasks in `IDENTIFY`. |
| `discord_remove_thread_member` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_request_guild_members` | `include/discord-events.h` | supported | Sends Fluxer Gateway opcode `8` (`RequestGuildMembers`) with decimal-string `guild_id` / `user_ids`; chunks arrive via `GUILD_MEMBERS_CHUNK`. |
| `discord_run` | `include/discord.h` | adapted | Connects to the discovered Fluxer Gateway (`?v=1&encoding=json` after `GET /v1/gateway/bot`) and runs the main event, timer, and REST loop. |
| `discord_search_guild_members` | `include/guild.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_set_data` | `include/discord.h` | supported | Local setter for caller-supplied user data pointer. |
| `discord_set_event_scheduler` | `include/discord-events.h` | supported | Configures main-thread, worker-thread, or ignored dispatch scheduling for Fluxer Gateway events. |
| `discord_set_instance_origin` | `include/discord.h` | supported | Sets the Fluxer HTTP(S) instance origin before `discord_run()` and refreshes endpoints via `GET /.well-known/fluxer`. |
| `discord_set_on_application_command_permissions_update` | `include/discord-events.h` | unsupported | Fluxer currently has no compatible application-command permission event. |
| `discord_set_on_auto_moderation_action_execution` | `include/discord-events.h` | unsupported | Fluxer Gateway v1 does not emit `AUTO_MODERATION_ACTION_EXECUTION`. |
| `discord_set_on_auto_moderation_rule_create` | `include/discord-events.h` | unsupported | Fluxer Gateway v1 does not emit `AUTO_MODERATION_RULE_CREATE`. |
| `discord_set_on_auto_moderation_rule_delete` | `include/discord-events.h` | unsupported | Fluxer Gateway v1 does not emit `AUTO_MODERATION_RULE_DELETE`. |
| `discord_set_on_auto_moderation_rule_update` | `include/discord-events.h` | unsupported | Fluxer Gateway v1 does not emit `AUTO_MODERATION_RULE_UPDATE`. |
| `discord_set_on_channel_create` | `include/discord-events.h` | supported | Fluxer Gateway v1 `CHANNEL_CREATE` dispatch event. |
| `discord_set_on_channel_delete` | `include/discord-events.h` | supported | Fluxer Gateway v1 `CHANNEL_DELETE` dispatch event. |
| `discord_set_on_channel_pins_update` | `include/discord-events.h` | supported | Fluxer Gateway v1 `CHANNEL_PINS_UPDATE` dispatch event. |
| `discord_set_on_channel_update` | `include/discord-events.h` | supported | Fluxer Gateway v1 `CHANNEL_UPDATE` dispatch event. |
| `discord_set_on_command` | `include/discord-events.h` | adapted | Prefix-command router evaluated over Fluxer `MESSAGE_CREATE` payloads without Discord `MESSAGE_CONTENT` intents. |
| `discord_set_on_commands` | `include/discord-events.h` | adapted | Multi-command prefix router evaluated over Fluxer `MESSAGE_CREATE` payloads without Discord `MESSAGE_CONTENT` intents. |
| `discord_set_on_cycle` | `include/discord-events.h` | supported | Local Concord event-loop cycle callback. |
| `discord_set_on_guild_ban_add` | `include/discord-events.h` | supported | Fluxer Gateway v1 `GUILD_BAN_ADD` dispatch event. |
| `discord_set_on_guild_ban_remove` | `include/discord-events.h` | supported | Fluxer Gateway v1 `GUILD_BAN_REMOVE` dispatch event. |
| `discord_set_on_guild_create` | `include/discord-events.h` | supported | Fluxer Gateway v1 `GUILD_CREATE` dispatch event. |
| `discord_set_on_guild_delete` | `include/discord-events.h` | supported | Fluxer Gateway v1 `GUILD_DELETE` dispatch event. |
| `discord_set_on_guild_emojis_update` | `include/discord-events.h` | supported | Fluxer Gateway v1 `GUILD_EMOJIS_UPDATE` dispatch event. |
| `discord_set_on_guild_integrations_update` | `include/discord-events.h` | unsupported | Fluxer Gateway v1 does not emit `GUILD_INTEGRATIONS_UPDATE`. |
| `discord_set_on_guild_member_add` | `include/discord-events.h` | supported | Fluxer Gateway v1 `GUILD_MEMBER_ADD` dispatch event. |
| `discord_set_on_guild_member_remove` | `include/discord-events.h` | supported | Fluxer Gateway v1 `GUILD_MEMBER_REMOVE` dispatch event. |
| `discord_set_on_guild_members_chunk` | `include/discord-events.h` | supported | Fluxer Gateway v1 `GUILD_MEMBERS_CHUNK` dispatch event. |
| `discord_set_on_guild_member_update` | `include/discord-events.h` | supported | Fluxer Gateway v1 `GUILD_MEMBER_UPDATE` dispatch event. |
| `discord_set_on_guild_role_create` | `include/discord-events.h` | supported | Fluxer Gateway v1 `GUILD_ROLE_CREATE` dispatch event. |
| `discord_set_on_guild_role_delete` | `include/discord-events.h` | supported | Fluxer Gateway v1 `GUILD_ROLE_DELETE` dispatch event. |
| `discord_set_on_guild_role_update` | `include/discord-events.h` | supported | Fluxer Gateway v1 `GUILD_ROLE_UPDATE` dispatch event. |
| `discord_set_on_guild_scheduled_event_create` | `include/discord-events.h` | unsupported | Fluxer Gateway v1 does not emit `GUILD_SCHEDULED_EVENT_CREATE`. |
| `discord_set_on_guild_scheduled_event_delete` | `include/discord-events.h` | unsupported | Fluxer Gateway v1 does not emit `GUILD_SCHEDULED_EVENT_DELETE`. |
| `discord_set_on_guild_scheduled_event_update` | `include/discord-events.h` | unsupported | Fluxer Gateway v1 does not emit `GUILD_SCHEDULED_EVENT_UPDATE`. |
| `discord_set_on_guild_scheduled_event_user_add` | `include/discord-events.h` | unsupported | Fluxer Gateway v1 does not emit `GUILD_SCHEDULED_EVENT_USER_ADD`. |
| `discord_set_on_guild_scheduled_event_user_remove` | `include/discord-events.h` | unsupported | Fluxer Gateway v1 does not emit `GUILD_SCHEDULED_EVENT_USER_REMOVE`. |
| `discord_set_on_guild_stickers_update` | `include/discord-events.h` | supported | Fluxer Gateway v1 `GUILD_STICKERS_UPDATE` dispatch event. |
| `discord_set_on_guild_update` | `include/discord-events.h` | supported | Fluxer Gateway v1 `GUILD_UPDATE` dispatch event. |
| `discord_set_on_idle` | `include/discord-events.h` | supported | Local Concord event-loop idle callback. |
| `discord_set_on_integration_create` | `include/discord-events.h` | unsupported | Fluxer Gateway v1 does not emit `INTEGRATION_CREATE`. |
| `discord_set_on_integration_delete` | `include/discord-events.h` | unsupported | Fluxer Gateway v1 does not emit `INTEGRATION_DELETE`. |
| `discord_set_on_integration_update` | `include/discord-events.h` | unsupported | Fluxer Gateway v1 does not emit `INTEGRATION_UPDATE`. |
| `discord_set_on_interaction_create` | `include/discord-events.h` | unsupported | Fluxer currently has no compatible slash-command interaction event. |
| `discord_set_on_invite_create` | `include/discord-events.h` | supported | Fluxer Gateway v1 `INVITE_CREATE` dispatch event. |
| `discord_set_on_invite_delete` | `include/discord-events.h` | supported | Fluxer Gateway v1 `INVITE_DELETE` dispatch event. |
| `discord_set_on_message_create` | `include/discord-events.h` | supported | Fluxer Gateway v1 `MESSAGE_CREATE` dispatch event. |
| `discord_set_on_message_delete` | `include/discord-events.h` | supported | Fluxer Gateway v1 `MESSAGE_DELETE` dispatch event. |
| `discord_set_on_message_delete_bulk` | `include/discord-events.h` | supported | Fluxer Gateway v1 `MESSAGE_DELETE_BULK` dispatch event. |
| `discord_set_on_message_reaction_add` | `include/discord-events.h` | supported | Fluxer Gateway v1 `MESSAGE_REACTION_ADD` dispatch event. |
| `discord_set_on_message_reaction_remove` | `include/discord-events.h` | supported | Fluxer Gateway v1 `MESSAGE_REACTION_REMOVE` dispatch event. |
| `discord_set_on_message_reaction_remove_all` | `include/discord-events.h` | supported | Fluxer Gateway v1 `MESSAGE_REACTION_REMOVE_ALL` dispatch event. |
| `discord_set_on_message_reaction_remove_emoji` | `include/discord-events.h` | supported | Fluxer Gateway v1 `MESSAGE_REACTION_REMOVE_EMOJI` dispatch event. |
| `discord_set_on_message_update` | `include/discord-events.h` | supported | Fluxer Gateway v1 `MESSAGE_UPDATE` dispatch event. |
| `discord_set_on_presence_update` | `include/discord-events.h` | supported | Fluxer Gateway v1 `PRESENCE_UPDATE` dispatch event. |
| `discord_set_on_ready` | `include/discord-events.h` | supported | Fluxer Gateway v1 `READY` dispatch event. |
| `discord_set_on_stage_instance_create` | `include/discord-events.h` | unsupported | Fluxer Gateway v1 does not emit `STAGE_INSTANCE_CREATE`. |
| `discord_set_on_stage_instance_delete` | `include/discord-events.h` | unsupported | Fluxer Gateway v1 does not emit `STAGE_INSTANCE_DELETE`. |
| `discord_set_on_stage_instance_update` | `include/discord-events.h` | unsupported | Fluxer Gateway v1 does not emit `STAGE_INSTANCE_UPDATE`. |
| `discord_set_on_thread_create` | `include/discord-events.h` | supported | Fluxer Gateway v1 `THREAD_CREATE` dispatch event. |
| `discord_set_on_thread_delete` | `include/discord-events.h` | supported | Fluxer Gateway v1 `THREAD_DELETE` dispatch event. |
| `discord_set_on_thread_list_sync` | `include/discord-events.h` | supported | Fluxer Gateway v1 `THREAD_LIST_SYNC` dispatch event. |
| `discord_set_on_thread_members_update` | `include/discord-events.h` | supported | Fluxer Gateway v1 `THREAD_MEMBERS_UPDATE` dispatch event. |
| `discord_set_on_thread_member_update` | `include/discord-events.h` | supported | Fluxer Gateway v1 `THREAD_MEMBER_UPDATE` dispatch event. |
| `discord_set_on_thread_update` | `include/discord-events.h` | supported | Fluxer Gateway v1 `THREAD_UPDATE` dispatch event. |
| `discord_set_on_typing_start` | `include/discord-events.h` | adapted | Fluxer Gateway v1 `TYPING_START` dispatch event; adapts numeric Unix `timestamp` seconds into `u64unix_ms`. |
| `discord_set_on_user_update` | `include/discord-events.h` | supported | Fluxer Gateway v1 `USER_UPDATE` dispatch event. |
| `discord_set_on_voice_server_update` | `include/discord-events.h` | supported | Fluxer Gateway v1 `VOICE_SERVER_UPDATE` dispatch event. |
| `discord_set_on_voice_state_update` | `include/discord-events.h` | supported | Fluxer Gateway v1 `VOICE_STATE_UPDATE` dispatch event. |
| `discord_set_on_webhooks_update` | `include/discord-events.h` | supported | Fluxer Gateway v1 `WEBHOOKS_UPDATE` dispatch event. |
| `discord_set_prefix` | `include/discord-events.h` | supported | Configures mandatory prefix for Concord's local `MESSAGE_CREATE` command router. |
| `discord_shutdown` | `include/discord.h` | supported | Gracefully closes the active Fluxer Gateway session and exits `discord_run()`. |
| `discord_shutdown_all` | `include/discord.h` | supported | Broadcasts graceful shutdown to all running clients in the process. |
| `discord_shutdown_all_ongoing` | `include/discord.h` | supported | Reports whether a process-wide client shutdown notification is active. |
| `discord_start_thread_with_message` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_start_thread_without_message` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_strerror` | `include/discord.h` | supported | Local human-readable error formatter (including `CCORD_UNSUPPORTED`). |
| `discord_sync_guild_template` | `include/guild_template.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_timer` | `include/discord.h` | supported | Local one-shot millisecond timer scheduler on the client event loop. |
| `discord_timer_cancel` | `include/discord.h` | supported | Local timer cancellation helper. |
| `discord_timer_cancel_and_delete` | `include/discord.h` | supported | Local timer cancellation and deletion helper. |
| `discord_timer_ctl` | `include/discord.h` | supported | Local low-level timer creation and control API. |
| `discord_timer_delete` | `include/discord.h` | supported | Local timer deletion helper. |
| `discord_timer_get` | `include/discord.h` | supported | Local timer inspection helper. |
| `discord_timer_interval` | `include/discord.h` | supported | Local repeating interval timer scheduler. |
| `discord_timer_start` | `include/discord.h` | supported | Local timer start helper. |
| `discord_timer_stop` | `include/discord.h` | supported | Local timer stop helper. |
| `discord_timestamp` | `include/discord.h` | supported | Local timestamp helper in milliseconds. |
| `discord_timestamp_us` | `include/discord.h` | supported | Local timestamp helper in microseconds. |
| `discord_trigger_typing_indicator` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_unclaim` | `include/discord.h` | supported | Local reference-counting helper releasing a claimed Concord resource. |
| `discord_unpin_message` | `include/channel.h` | pending | Map against the documented Fluxer endpoint/event in its resource task. |
| `discord_update_presence` | `include/discord-events.h` | adapted | Sends Fluxer Gateway opcode `3` (`PresenceUpdate`) with numeric millisecond/`null` `since`, `activities`, `status`, and `afk`. |
| `discord_update_voice_state` | `include/discord-events.h` | adapted | Sends Fluxer Gateway opcode `4` (`VoiceStateUpdate`) with decimal-string `guild_id` / `channel_id` (`null` on disconnect). |

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "discord.h"
#include "discord-internal.h"

#define GATEWAY_CB(EV)    client->gw.cbs[1][EV]
#define ASSIGN_CB(EV, CB) GATEWAY_CB(EV) = (discord_ev_event)CB

void
discord_shutdown(struct discord *client)
{
    if (!client || !client->gw.session) return;
    if (client->gw.session->status != DISCORD_SESSION_SHUTDOWN)
        discord_gateway_shutdown(&client->gw);
}

void
discord_reconnect(struct discord *client, bool resume)
{
    if (!client) return;
    discord_gateway_reconnect(&client->gw, resume);
}

void
discord_request_guild_members(struct discord *client,
                              struct discord_request_guild_members *request)
{
    if (!client || !request) return;
    if (!GATEWAY_CB(DISCORD_EV_GUILD_MEMBERS_CHUNK)) {
        logmod_log(
            ERROR, client->logger,
            "Missing callback for discord_set_on_guild_members_chunk()");
        return;
    }
    discord_gateway_send_request_guild_members(&client->gw, request);
}

void
discord_update_voice_state(struct discord *client,
                           struct discord_update_voice_state *update)
{
    if (!client || !update) return;
    discord_gateway_send_update_voice_state(&client->gw, update);
}

void
discord_update_presence(struct discord *client,
                        struct discord_presence_update *presence)
{
    if (!client || !presence) return;
    discord_gateway_send_presence_update(&client->gw, presence);
}

void
discord_add_intents(struct discord *client, uint64_t code)
{
    if (!client) return;
    const struct logmod_logger *logger = client->logger;
    if (client->gw.ws && WS_CONNECTED == ws_get_status(client->gw.ws)) {
        logmod_log(ERROR, logger, "Can't set intents to a running client.");
        return;
    }

    client->gw.id.intents |= code;
}

void
discord_remove_intents(struct discord *client, uint64_t code)
{
    if (!client) return;
    const struct logmod_logger *logger = client->logger;
    if (client->gw.ws && WS_CONNECTED == ws_get_status(client->gw.ws)) {
        logmod_log(ERROR, logger,
                   "Can't remove intents from a running client.");
        return;
    }

    client->gw.id.intents &= ~code;
}

void
discord_set_prefix(struct discord *client, const char prefix[])
{
    if (!client || !prefix || !*prefix) return;

    discord_message_commands_set_prefix(&client->commands, prefix,
                                        strlen(prefix));
}

void
discord_set_event_scheduler(struct discord *client, discord_ev_scheduler cb)
{
    if (!client) return;
    client->gw.scheduler = cb;
}

void
discord_set_on_command(struct discord *client,
                       const char command[],
                       void (*cb)(struct discord *client,
                                  const struct discord_message *event))
{
    if (!client) return;
    size_t length = (!command || !*command) ? 0 : strlen(command);

    discord_message_commands_append(&client->commands, command, length, cb);
}

void
discord_set_on_commands(struct discord *client,
                        const char *commands[],
                        int amount,
                        void (*cb)(struct discord *client,
                                   const struct discord_message *event))
{
    if (!client || !commands) return;
    for (int i = 0; i < amount; ++i)
        discord_set_on_command(client, commands[i], cb);
}

void
discord_set_on_ready(struct discord *client,
                     void (*cb)(struct discord *client,
                                const struct discord_ready *event))
{
    ASSIGN_CB(DISCORD_EV_READY, cb);
}

void
discord_set_on_application_command_permissions_update(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_application_command_permissions *event))
{
    logmod_log(
        WARN, client->logger,
        "Fluxer Gateway v1 does not emit APPLICATION_COMMAND_PERMISSIONS_UPDATE");
    ASSIGN_CB(DISCORD_EV_APPLICATION_COMMAND_PERMISSIONS_UPDATE, cb);
}

void
discord_set_on_auto_moderation_rule_create(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_auto_moderation_rule *event))
{
    logmod_log(WARN, client->logger,
               "Fluxer Gateway v1 does not emit AUTO_MODERATION_RULE_CREATE");
    ASSIGN_CB(DISCORD_EV_AUTO_MODERATION_RULE_CREATE, cb);
}

void
discord_set_on_auto_moderation_rule_update(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_auto_moderation_rule *event))
{
    logmod_log(WARN, client->logger,
               "Fluxer Gateway v1 does not emit AUTO_MODERATION_RULE_UPDATE");
    ASSIGN_CB(DISCORD_EV_AUTO_MODERATION_RULE_UPDATE, cb);
}

void
discord_set_on_auto_moderation_rule_delete(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_auto_moderation_rule *event))
{
    logmod_log(WARN, client->logger,
               "Fluxer Gateway v1 does not emit AUTO_MODERATION_RULE_DELETE");
    ASSIGN_CB(DISCORD_EV_AUTO_MODERATION_RULE_DELETE, cb);
}

void
discord_set_on_auto_moderation_action_execution(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_auto_moderation_action_execution *event))
{
    logmod_log(
        WARN, client->logger,
        "Fluxer Gateway v1 does not emit AUTO_MODERATION_ACTION_EXECUTION");
    ASSIGN_CB(DISCORD_EV_AUTO_MODERATION_ACTION_EXECUTION, cb);
}

void
discord_set_on_channel_create(struct discord *client,
                              void (*cb)(struct discord *client,
                                         const struct discord_channel *event))
{
    ASSIGN_CB(DISCORD_EV_CHANNEL_CREATE, cb);
}

void
discord_set_on_channel_update(struct discord *client,
                              void (*cb)(struct discord *client,
                                         const struct discord_channel *event))
{
    ASSIGN_CB(DISCORD_EV_CHANNEL_UPDATE, cb);
}

void
discord_set_on_channel_delete(struct discord *client,
                              void (*cb)(struct discord *client,
                                         const struct discord_channel *event))
{
    ASSIGN_CB(DISCORD_EV_CHANNEL_DELETE, cb);
}

void
discord_set_on_channel_pins_update(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_channel_pins_update *event))
{
    ASSIGN_CB(DISCORD_EV_CHANNEL_PINS_UPDATE, cb);
}

void
discord_set_on_thread_create(struct discord *client,
                             void (*cb)(struct discord *client,
                                        const struct discord_channel *event))
{
    ASSIGN_CB(DISCORD_EV_THREAD_CREATE, cb);
}

void
discord_set_on_thread_update(struct discord *client,
                             void (*cb)(struct discord *client,
                                        const struct discord_channel *event))
{
    ASSIGN_CB(DISCORD_EV_THREAD_UPDATE, cb);
}

void
discord_set_on_thread_delete(struct discord *client,
                             void (*cb)(struct discord *client,
                                        const struct discord_channel *event))
{
    ASSIGN_CB(DISCORD_EV_THREAD_DELETE, cb);
}

void
discord_set_on_thread_list_sync(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_thread_list_sync *event))
{
    ASSIGN_CB(DISCORD_EV_THREAD_LIST_SYNC, cb);
}

void
discord_set_on_thread_member_update(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_thread_member *event))
{
    ASSIGN_CB(DISCORD_EV_THREAD_MEMBER_UPDATE, cb);
}

void
discord_set_on_thread_members_update(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_thread_members_update *event))
{
    ASSIGN_CB(DISCORD_EV_THREAD_MEMBERS_UPDATE, cb);
}

void
discord_set_on_guild_create(struct discord *client,
                            void (*cb)(struct discord *client,
                                       const struct discord_guild *event))
{
    ASSIGN_CB(DISCORD_EV_GUILD_CREATE, cb);
}

void
discord_set_on_guild_update(struct discord *client,
                            void (*cb)(struct discord *client,
                                       const struct discord_guild *event))
{
    ASSIGN_CB(DISCORD_EV_GUILD_UPDATE, cb);
}

void
discord_set_on_guild_delete(struct discord *client,
                            void (*cb)(struct discord *client,
                                       const struct discord_guild *event))
{
    ASSIGN_CB(DISCORD_EV_GUILD_DELETE, cb);
}

void
discord_set_on_guild_ban_add(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_guild_ban_add *event))
{
    ASSIGN_CB(DISCORD_EV_GUILD_BAN_ADD, cb);
}

void
discord_set_on_guild_ban_remove(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_guild_ban_remove *event))
{
    ASSIGN_CB(DISCORD_EV_GUILD_BAN_REMOVE, cb);
}

void
discord_set_on_guild_emojis_update(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_guild_emojis_update *event))
{
    ASSIGN_CB(DISCORD_EV_GUILD_EMOJIS_UPDATE, cb);
}

void
discord_set_on_guild_stickers_update(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_guild_stickers_update *event))
{
    ASSIGN_CB(DISCORD_EV_GUILD_STICKERS_UPDATE, cb);
}

void
discord_set_on_guild_integrations_update(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_guild_integrations_update *event))
{
    logmod_log(WARN, client->logger,
               "Fluxer Gateway v1 does not emit GUILD_INTEGRATIONS_UPDATE");
    ASSIGN_CB(DISCORD_EV_GUILD_INTEGRATIONS_UPDATE, cb);
}

void
discord_set_on_guild_member_add(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_guild_member *event))
{
    ASSIGN_CB(DISCORD_EV_GUILD_MEMBER_ADD, cb);
}

void
discord_set_on_guild_member_update(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_guild_member_update *event))
{
    ASSIGN_CB(DISCORD_EV_GUILD_MEMBER_UPDATE, cb);
}

void
discord_set_on_guild_member_remove(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_guild_member_remove *event))
{
    ASSIGN_CB(DISCORD_EV_GUILD_MEMBER_REMOVE, cb);
}

void
discord_set_on_guild_members_chunk(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_guild_members_chunk *event))
{
    ASSIGN_CB(DISCORD_EV_GUILD_MEMBERS_CHUNK, cb);
}

void
discord_set_on_guild_role_create(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_guild_role_create *event))
{
    ASSIGN_CB(DISCORD_EV_GUILD_ROLE_CREATE, cb);
}

void
discord_set_on_guild_role_update(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_guild_role_update *event))
{
    ASSIGN_CB(DISCORD_EV_GUILD_ROLE_UPDATE, cb);
}

void
discord_set_on_guild_role_delete(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_guild_role_delete *event))
{
    ASSIGN_CB(DISCORD_EV_GUILD_ROLE_DELETE, cb);
}

void
discord_set_on_guild_scheduled_event_create(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_guild_scheduled_event *event))
{
    logmod_log(WARN, client->logger,
               "Fluxer Gateway v1 does not emit GUILD_SCHEDULED_EVENT_CREATE");
    ASSIGN_CB(DISCORD_EV_GUILD_SCHEDULED_EVENT_CREATE, cb);
}

void
discord_set_on_guild_scheduled_event_update(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_guild_scheduled_event *event))
{
    logmod_log(WARN, client->logger,
               "Fluxer Gateway v1 does not emit GUILD_SCHEDULED_EVENT_UPDATE");
    ASSIGN_CB(DISCORD_EV_GUILD_SCHEDULED_EVENT_UPDATE, cb);
}

void
discord_set_on_guild_scheduled_event_delete(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_guild_scheduled_event *event))
{
    logmod_log(WARN, client->logger,
               "Fluxer Gateway v1 does not emit GUILD_SCHEDULED_EVENT_DELETE");
    ASSIGN_CB(DISCORD_EV_GUILD_SCHEDULED_EVENT_DELETE, cb);
}

void
discord_set_on_guild_scheduled_event_user_add(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_guild_scheduled_event_user_add *event))
{
    logmod_log(
        WARN, client->logger,
        "Fluxer Gateway v1 does not emit GUILD_SCHEDULED_EVENT_USER_ADD");
    ASSIGN_CB(DISCORD_EV_GUILD_SCHEDULED_EVENT_USER_ADD, cb);
}

void
discord_set_on_guild_scheduled_event_user_remove(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_guild_scheduled_event_user_remove *event))
{
    logmod_log(
        WARN, client->logger,
        "Fluxer Gateway v1 does not emit GUILD_SCHEDULED_EVENT_USER_REMOVE");
    ASSIGN_CB(DISCORD_EV_GUILD_SCHEDULED_EVENT_USER_REMOVE, cb);
}

void
discord_set_on_integration_create(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_integration *event))
{
    logmod_log(WARN, client->logger,
               "Fluxer Gateway v1 does not emit INTEGRATION_CREATE");
    ASSIGN_CB(DISCORD_EV_INTEGRATION_CREATE, cb);
}

void
discord_set_on_integration_update(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_integration *event))
{
    logmod_log(WARN, client->logger,
               "Fluxer Gateway v1 does not emit INTEGRATION_UPDATE");
    ASSIGN_CB(DISCORD_EV_INTEGRATION_UPDATE, cb);
}

void
discord_set_on_integration_delete(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_integration_delete *event))
{
    logmod_log(WARN, client->logger,
               "Fluxer Gateway v1 does not emit INTEGRATION_DELETE");
    ASSIGN_CB(DISCORD_EV_INTEGRATION_DELETE, cb);
}

void
discord_set_on_interaction_create(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_interaction *event))
{
    logmod_log(WARN, client->logger,
               "Fluxer Gateway v1 does not emit INTERACTION_CREATE");
    ASSIGN_CB(DISCORD_EV_INTERACTION_CREATE, cb);
}

void
discord_set_on_invite_create(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_invite_create *event))
{
    ASSIGN_CB(DISCORD_EV_INVITE_CREATE, cb);
}

void
discord_set_on_invite_delete(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_invite_delete *event))
{
    ASSIGN_CB(DISCORD_EV_INVITE_DELETE, cb);
}

void
discord_set_on_message_create(struct discord *client,
                              void (*cb)(struct discord *client,
                                         const struct discord_message *event))
{
    ASSIGN_CB(DISCORD_EV_MESSAGE_CREATE, cb);
}

void
discord_set_on_message_update(struct discord *client,
                              void (*cb)(struct discord *client,
                                         const struct discord_message *event))
{
    ASSIGN_CB(DISCORD_EV_MESSAGE_UPDATE, cb);
}

void
discord_set_on_message_delete(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_message_delete *event))
{
    ASSIGN_CB(DISCORD_EV_MESSAGE_DELETE, cb);
}

void
discord_set_on_message_delete_bulk(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_message_delete_bulk *event))
{
    ASSIGN_CB(DISCORD_EV_MESSAGE_DELETE_BULK, cb);
}

void
discord_set_on_message_reaction_add(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_message_reaction_add *event))
{
    ASSIGN_CB(DISCORD_EV_MESSAGE_REACTION_ADD, cb);
}

void
discord_set_on_message_reaction_remove(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_message_reaction_remove *event))
{
    ASSIGN_CB(DISCORD_EV_MESSAGE_REACTION_REMOVE, cb);
}

void
discord_set_on_message_reaction_remove_all(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_message_reaction_remove_all *event))
{
    ASSIGN_CB(DISCORD_EV_MESSAGE_REACTION_REMOVE_ALL, cb);
}

void
discord_set_on_message_reaction_remove_emoji(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_message_reaction_remove_emoji *event))
{
    ASSIGN_CB(DISCORD_EV_MESSAGE_REACTION_REMOVE_EMOJI, cb);
}

void
discord_set_on_presence_update(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_presence_update *event))
{
    ASSIGN_CB(DISCORD_EV_PRESENCE_UPDATE, cb);
}

void
discord_set_on_stage_instance_create(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_stage_instance *event))
{
    logmod_log(WARN, client->logger,
               "Fluxer Gateway v1 does not emit STAGE_INSTANCE_CREATE");
    ASSIGN_CB(DISCORD_EV_STAGE_INSTANCE_CREATE, cb);
}

void
discord_set_on_stage_instance_update(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_stage_instance *event))
{
    logmod_log(WARN, client->logger,
               "Fluxer Gateway v1 does not emit STAGE_INSTANCE_UPDATE");
    ASSIGN_CB(DISCORD_EV_STAGE_INSTANCE_UPDATE, cb);
}

void
discord_set_on_stage_instance_delete(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_stage_instance *event))
{
    logmod_log(WARN, client->logger,
               "Fluxer Gateway v1 does not emit STAGE_INSTANCE_DELETE");
    ASSIGN_CB(DISCORD_EV_STAGE_INSTANCE_DELETE, cb);
}

void
discord_set_on_typing_start(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_typing_start *event))
{
    ASSIGN_CB(DISCORD_EV_TYPING_START, cb);
}

void
discord_set_on_user_update(struct discord *client,
                           void (*cb)(struct discord *client,
                                      const struct discord_user *event))
{
    ASSIGN_CB(DISCORD_EV_USER_UPDATE, cb);
}

void
discord_set_on_voice_state_update(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_voice_state *event))
{
    ASSIGN_CB(DISCORD_EV_VOICE_STATE_UPDATE, cb);
}

void
discord_set_on_voice_server_update(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_voice_server_update *event))
{
    ASSIGN_CB(DISCORD_EV_VOICE_SERVER_UPDATE, cb);
}

void
discord_set_on_webhooks_update(
    struct discord *client,
    void (*cb)(struct discord *client,
               const struct discord_webhooks_update *event))
{
    ASSIGN_CB(DISCORD_EV_WEBHOOKS_UPDATE, cb);
}

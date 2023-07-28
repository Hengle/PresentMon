// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: MIT
import { Module, VuexModule, Mutation, Action, getModule  } from 'vuex-module-decorators'
import store from './index'
import { Notification } from '@/core/notification'
import { delayFor } from '@/core/timing'

@Module({name: 'notifications', dynamic: true, store, namespaced: true})
export class NotificationsModule extends VuexModule {
  notifications: Notification[] = [];
  showing = false;

  @Mutation
  push(notification: Notification) {
    this.notifications.push(notification);
  }

  @Mutation
  pop() {
    if (this.notifications.length > 0) {
      this.notifications.shift();
    }
  }

  @Mutation
  show() {
    this.showing = true;
  }

  @Mutation
  hide() {
    this.showing = false;
  }

  get current(): Notification|null {
    if (this.notifications.length > 0) {
      return this.notifications[0];
    } else {
      return null;
    }
  }

  get count(): number {
    return this.notifications.length;
  }

  @Action  
  async notify(note: Notification) {
    this.context.commit('push', note);
    this.context.commit('show');
  }

  @Action  
  async dismiss() {
    this.context.commit('hide');
    await delayFor(250);
    this.context.commit('pop');
    if (this.notifications.length > 0) {
      this.context.commit('show');
    }
  }
}

export const Notifications = getModule(NotificationsModule);
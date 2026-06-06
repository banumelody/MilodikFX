import { messageBridge } from './messageBridge';
import { eventDispatcher } from './eventDispatcher';

export interface EffectInstance {
  id: string;
  type: string;
  name: string;
  enabled: boolean;
  position: number;
  parameters: Record<string, number>;
}

class EffectManager {
  private effects: Map<string, EffectInstance> = new Map();

  async addEffect(type: string, position: number = -1): Promise<EffectInstance> {
    try {
      const response = await messageBridge.sendMessage('effect.add', {
        type,
        position: position === -1 ? this.effects.size : position,
      });

      const effect: EffectInstance = {
        id: response.effectId,
        type,
        name: this.getEffectName(type),
        enabled: true,
        position: response.position,
        parameters: {},
      };

      this.effects.set(effect.id, effect);
      eventDispatcher.emit('effect:added', effect);
      return effect;
    } catch (error) {
      eventDispatcher.emit('error', { message: `Failed to add effect: ${error}` });
      throw error;
    }
  }

  async removeEffect(effectId: string): Promise<void> {
    try {
      await messageBridge.sendMessage('effect.remove', { id: effectId });
      this.effects.delete(effectId);
      eventDispatcher.emit('effect:removed', { effectId });
    } catch (error) {
      eventDispatcher.emit('error', { message: `Failed to remove effect: ${error}` });
      throw error;
    }
  }

  async reorderEffects(effectIds: string[]): Promise<void> {
    try {
      await messageBridge.sendMessage('effect.reorder', effectIds);
      eventDispatcher.emit('effect:reordered', { order: effectIds });
    } catch (error) {
      eventDispatcher.emit('error', { message: `Failed to reorder effects: ${error}` });
      throw error;
    }
  }

  async setEffectParameter(
    effectId: string,
    paramName: string,
    value: number,
  ): Promise<void> {
    try {
      await messageBridge.sendMessage('parameter.set', {
        effectId,
        paramName,
        value,
      });

      const effect = this.effects.get(effectId);
      if (effect) {
        effect.parameters[paramName] = value;
        eventDispatcher.emit('parameter:changed', {
          effectId,
          paramName,
          value,
        });
      }
    } catch (error) {
      eventDispatcher.emit('error', { message: `Failed to set parameter: ${error}` });
      throw error;
    }
  }

  async toggleEffect(effectId: string): Promise<void> {
    const effect = this.effects.get(effectId);
    if (!effect) throw new Error(`Effect ${effectId} not found`);

    try {
      effect.enabled = !effect.enabled;
      await messageBridge.sendMessage('effect.toggle', {
        id: effectId,
        enabled: effect.enabled,
      });
      eventDispatcher.emit('effect:toggled', { effectId, enabled: effect.enabled });
    } catch (error) {
      effect.enabled = !effect.enabled;
      eventDispatcher.emit('error', { message: `Failed to toggle effect: ${error}` });
      throw error;
    }
  }

  getEffect(effectId: string): EffectInstance | undefined {
    return this.effects.get(effectId);
  }

  getAllEffects(): EffectInstance[] {
    return Array.from(this.effects.values()).sort((a, b) => a.position - b.position);
  }

  private getEffectName(type: string): string {
    const names: Record<string, string> = {
      gain: 'Gain',
      overdrive: 'Overdrive',
      eq: 'EQ',
      compressor: 'Compressor',
      reverb: 'Reverb',
      'tone-stack': 'Tone Stack',
    };
    return names[type] || type;
  }
}

export const effectManager = new EffectManager();

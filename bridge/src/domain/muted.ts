import { hasMutedWord } from '@atproto/api'
import type { Agent, AppBskyActorDefs } from '@atproto/api'

/**
 * Muted words are applied here, before a response is built, so the Mac never
 * downloads a post it would have to hide. Matching is the SDK's own
 * (`hasMutedWord`), so the rules (whole-word for short words, tags, expiry,
 * "exclude following") are Bluesky's and not a copy of them.
 */
const CACHE_MS = 60_000
export const MAX_MUTED_WORD_LENGTH = 100

export interface MutedWordView {
  value: string
  /** "content" and/or "tag". */
  targets: string[]
}

type Pref = AppBskyActorDefs.MutedWord

export function validMutedWord(value: unknown): string | undefined {
  if (typeof value !== 'string') return undefined
  const v = value.trim()
  if (v.length === 0 || Array.from(v).length > MAX_MUTED_WORD_LENGTH) return undefined
  // eslint-disable-next-line no-control-regex
  if (/[\u0000-\u001f\u007f]/.test(v)) return undefined
  return v
}

interface PostForMuting {
  author: { did: string; viewer?: { following?: string } }
  record: unknown
}

export function isMutedPost(words: Pref[], post: PostForMuting): boolean {
  if (words.length === 0) return false
  const record = post.record !== null && typeof post.record === 'object'
    ? (post.record as { text?: unknown; facets?: unknown; tags?: unknown; langs?: unknown })
    : {}
  return hasMutedWord({
    mutedWords: words,
    text: typeof record.text === 'string' ? record.text : '',
    facets: Array.isArray(record.facets) ? (record.facets as never) : undefined,
    outlineTags: Array.isArray(record.tags) ? record.tags.filter((t): t is string => typeof t === 'string') : undefined,
    languages: Array.isArray(record.langs) ? record.langs.filter((t): t is string => typeof t === 'string') : undefined,
    actor: post.author as never,
  })
}

export class MutedWords {
  private cache = new Map<string, { at: number; words: Pref[] }>()

  private key(agent: Agent): string {
    return (agent as unknown as { accountDid?: string }).accountDid ?? ''
  }

  /** The account's muted words. A preferences failure shows posts unfiltered rather than none. */
  async load(agent: Agent): Promise<Pref[]> {
    const key = this.key(agent)
    const hit = this.cache.get(key)
    if (hit && Date.now() - hit.at < CACHE_MS) return hit.words
    let words: Pref[] = []
    try {
      const prefs = await agent.getPreferences()
      words = prefs.moderationPrefs?.mutedWords ?? []
    } catch {
      console.warn('Could not read muted words; showing posts unfiltered.')
      return []
    }
    this.cache.set(key, { at: Date.now(), words })
    return words
  }

  forget(agent: Agent): void {
    this.cache.delete(this.key(agent))
  }

  async list(agent: Agent): Promise<MutedWordView[]> {
    this.forget(agent)
    const words = await this.load(agent)
    return words.slice(0, 100).map(w => ({ value: w.value, targets: w.targets.filter(t => t === 'content' || t === 'tag') }))
  }

  /** Add or remove a word, idempotently. Matching on removal ignores case, as the AppView does. */
  async set(agent: Agent, value: string, on: boolean): Promise<{ value: string; on: boolean }> {
    this.forget(agent)
    const words = await this.load(agent)
    const existing = words.filter(w => w.value.toLowerCase() === value.toLowerCase())
    if (on && existing.length === 0) {
      await agent.addMutedWord({ value, targets: ['content', 'tag'], actorTarget: 'all' })
    } else if (!on && existing.length > 0) {
      await agent.removeMutedWords(existing)
    }
    this.forget(agent)
    return { value, on }
  }
}

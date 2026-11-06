import { defineCollection, z } from 'astro:content';
import { glob } from 'astro/loaders';

export const STATUS_LABELS = {
  concept: 'Concept',
  prototype: 'Prototype',
  'in-development': 'In development',
  implemented: 'Implemented',
  tested: 'Tested',
} as const;

const devlog = defineCollection({
  loader: glob({ pattern: '**/*.md', base: './src/content/devlog' }),
  schema: z
    .object({
      title: z.string(),
      date: z.coerce.date(),
      summary: z.string(),
      category: z.string(),
      tags: z.array(z.string()).default([]),
      // Only use "implemented" or "tested" when working builds or tests confirm it.
      status: z.enum(['concept', 'prototype', 'in-development', 'implemented', 'tested']),
      cover: z.string().optional(),
      coverAlt: z.string().optional(),
      draft: z.boolean().default(false),
    })
    .refine((post) => !post.cover || !!post.coverAlt, {
      message: 'coverAlt is required when a cover image is supplied',
    }),
});

export const collections = { devlog };

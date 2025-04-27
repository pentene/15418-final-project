import random
import argparse
import os
import math

def generate_operations(n_elements, n_operations, find_ratio, sameset_ratio, contention_level, output_filename):
    """
    Generates a file containing Union-Find operations (UNION=0, FIND=1, SAMESET=2).

    Args:
        n_elements (int): The number of elements (0 to n_elements-1).
        n_operations (int): The total number of operations to generate.
        find_ratio (float): The desired ratio of FIND operations (0.0 to 1.0).
        sameset_ratio (float): The desired ratio of SAMESET operations *within*
                               the non-FIND operations (0.0 to 1.0).
        contention_level (float): Controls the focus of operations (0.0 low, 1.0 high).
                                  0.0: Ops spread across all elements.
                                  1.0: Ops focused on elements 0 and 1.
        output_filename (str): The path to the output file.
    """
    if not (0.0 <= find_ratio <= 1.0):
        raise ValueError("find_ratio must be between 0.0 and 1.0")
    if not (0.0 <= sameset_ratio <= 1.0):
        raise ValueError("sameset_ratio must be between 0.0 and 1.0")
    if not (0.0 <= contention_level <= 1.0):
        raise ValueError("contention_level must be between 0.0 and 1.0")
    if n_elements <= 0:
        raise ValueError("n_elements must be positive")
    if n_operations <= 0:
        raise ValueError("n_operations must be positive")

    # --- Calculate Hotspot Range based on Contention ---
    # Higher contention means smaller hotspot range (focused near index 0)
    # We ensure the hotspot size is at least 2 (if n_elements >= 2)
    # to allow for non-trivial UNION/SAMESET ops.
    if n_elements == 1:
        hotspot_max_index = 0 # Only element 0 exists
        print("Note: n_elements is 1. Only FIND(0) operations are possible.")
        find_ratio = 1.0 # Force all ops to be FIND
    else:
        # Calculate the effective number of elements to target
        effective_elements = n_elements * (1.0 - contention_level)
        # Ensure at least 2 elements are targeted for high contention (if possible)
        hotspot_size = max(2, math.ceil(effective_elements))
        # Ensure hotspot size doesn't exceed n_elements
        hotspot_size = min(n_elements, hotspot_size)
        hotspot_max_index = hotspot_size - 1 # Max index is size - 1

    print(f"Generating {n_operations} operations for {n_elements} elements...")
    print(f"Target FIND ratio: {find_ratio:.2f}")
    print(f"Target SAMESET ratio (of non-FIND ops): {sameset_ratio:.2f}")
    print(f"Contention Level: {contention_level:.2f} -> Targeting elements in range [0, {hotspot_max_index}]")
    print(f"Output file: {output_filename}")

    # Ensure the output directory exists
    output_dir = os.path.dirname(output_filename)
    if output_dir and not os.path.exists(output_dir):
        os.makedirs(output_dir)
        print(f"Created directory: {output_dir}")

    try:
        with open(output_filename, 'w') as f:
            # Write header: <num_elements> <num_operations>
            f.write(f"{n_elements} {n_operations}\n")

            generated_ops_count = 0
            find_count = 0
            union_count = 0
            sameset_count = 0

            # Generate operations
            for _ in range(n_operations):
                op_type_val = -1 # Placeholder
                a = -1
                b = -1

                # Decide if FIND or non-FIND
                if random.random() < find_ratio:
                    # --- Generate FIND operation ---
                    op_type_val = 1
                    a = random.randint(0, hotspot_max_index)
                    # 'b' is ignored for FIND, can be anything (set to 0 for consistency)
                    b = 0
                    find_count += 1
                else:
                    # --- Generate non-FIND (UNION or SAMESET) ---
                    # Choose 'a' and 'b' from the hotspot range
                    a = random.randint(0, hotspot_max_index)
                    # Ensure b != a if possible (hotspot_max_index > 0)
                    if hotspot_max_index > 0:
                        b = random.randint(0, hotspot_max_index)
                        while a == b:
                            b = random.randint(0, hotspot_max_index)
                    else: # Only element 0 exists in hotspot
                        b = a # Must be the same as a

                    # Decide if UNION or SAMESET based on sameset_ratio
                    if random.random() < sameset_ratio:
                        op_type_val = 2 # SAMESET
                        sameset_count += 1
                    else:
                        op_type_val = 0 # UNION
                        union_count += 1

                # Write operation: <type> <a> <b>
                f.write(f"{op_type_val} {a} {b}\n")
                generated_ops_count += 1

            print("-" * 30)
            print(f"Successfully generated {generated_ops_count} operations.")
            actual_find_ratio = find_count / generated_ops_count if generated_ops_count > 0 else 0
            non_find_count = union_count + sameset_count
            actual_sameset_ratio = sameset_count / non_find_count if non_find_count > 0 else 0

            print(f"Actual FIND operations:    {find_count} ({actual_find_ratio:.4f})")
            print(f"Actual UNION operations:   {union_count}")
            print(f"Actual SAMESET operations: {sameset_count}")
            if non_find_count > 0:
                 print(f"  (SAMESET ratio of non-FIND: {actual_sameset_ratio:.4f})")
            print("-" * 30)

    except IOError as e:
        print(f"Error writing to file {output_filename}: {e}")
    except Exception as e:
        print(f"An unexpected error occurred: {e}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Generate operation files for Union-Find benchmarking (UNION=0, FIND=1, SAMESET=2).",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter # Show defaults in help
    )

    parser.add_argument("n_elements", type=int, help="Number of elements (e.g., 10000).")
    parser.add_argument("n_operations", type=int, help="Number of operations (e.g., 100000).")
    parser.add_argument("output_file", type=str, help="Path to the output operations file (e.g., datasets/ops_10k_100k_c0.5.txt).")
    parser.add_argument("--find-ratio", type=float, default=0.5,
                        help="Ratio of FIND operations (0.0 to 1.0).")
    parser.add_argument("--sameset-ratio", type=float, default=0.1,
                        help="Ratio of SAMESET operations *within the non-FIND* operations (0.0 to 1.0).")
    parser.add_argument("--contention-level", type=float, default=0.0,
                        help="Level of contention (0.0=low/spread, 1.0=high/focused).")
    parser.add_argument("--seed", type=int, default=None,
                        help="Optional random seed for reproducibility.")

    args = parser.parse_args()

    if args.seed is not None:
        random.seed(args.seed)
        print(f"Using random seed: {args.seed}")

    try:
        generate_operations(
            args.n_elements,
            args.n_operations,
            args.find_ratio,
            args.sameset_ratio,
            args.contention_level,
            args.output_file
        )
    except ValueError as e:
        print(f"Input Error: {e}")
    except Exception as e:
        print(f"An unexpected error occurred during generation: {e}")

